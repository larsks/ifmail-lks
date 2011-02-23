#!/usr/bin/perl
# TIC procesor for UN*X

push(@INC,$0=~m|^(.*)/[^/]+$|);

require "hacks.pl";

require "fcntl.ph";
require "stdio.ph";
require "errno.ph";
require "syslog.pl";

require "machdep.pl";
require "xfutil.pl";
require "misc.pl";
require "semaphore.pl";
require "ftn.pl";
require "ctlfiles.pl";

&utic();

sub utic
{
	&init();
	&semaphore_on();
	&process();
	&done();
	&semaphore_off();
}

sub init
{
	$CONFIG=$ARGV[0] if defined($ARGV[0]);
	&read_config();
	&openlog('u-tic','cons,pid',$LOG_FACILITY);
	#&syslog('info',"Startup");
	&read_areas();
	&read_passwd();
	&read_feeds();
	&build_subscription_fast();
	undef %PATTERNS;
	umask($UMASK);
	dbmopen(DBM_BASE,"$FILEBASE/fbdbm",0644) || die &syslog('err',"Cannot open $FILEBASE/fbdbm: $!") if $FILEBASE;
	&read_stats();
	&read_intab();
}

sub done
{
	dbmclose(DBM_BASE) if $FILEBASE;
	close(INEWS) if $INEWS_OPEN;
	&write_stats();
	#&syslog('info',"End");
	&closelog();
}

sub process
{
	local($tic,$file);

	foreach $tic (<$INBOUND/*.[Tt][Ii][Cc]>)
	{
		if (&read_tic($tic) && (!defined($TO) || &ftn_eq($TO,$ADDRESS)))
		{
			if (defined($FILE) && defined($CRC))
			{
				$FILE=~tr/A-Z/a-z/;
				$INBFILE=$FILE;
				unless ( -e "$INBOUND/$INBFILE" )
				{
					undef $INBFILE;
					foreach $file (<$INBOUND/*>)
					{
						$INBFILE=$file, last if -f $file && hex(`$EXT_CRC < '$file'`)==hex($CRC);
					}
					unless ($INBFILE)
					{
						&syslog('warning',"TIC w/o file: $tic - skipping");
						next;
					}
					$INBFILE=$1 if $INBFILE=~m|/([^/]+)$|;
				}
			}
	
			if (&check_tic($tic) && &check_pw($tic) && &check_area($tic) && &check_crc($tic))
			{
				&syslog('info',"Processing TIC ".&basename($tic)." from $FROM");
				&syslog('info',"AREA: $AREA FILE: $FILE");
				&make_path();
				&make_seenby();	
				&process_file();
				&xunlink($tic);
			}
			else
			{
				&move_bad($tic);
			}
		}
		else
		{
			#&syslog('notice',"Transit TICK from $FROM to $TO");
		}
	}
}

sub process_file
{
	local($link,$inb_file,$outb_file,$outb_dir,$tic,$outb,$flo,@linked,$bsy,$pid,$koidesc);

	$inb_file="$FE_OUTBOUND/tmp/$FILE";
	&xmkdir("$FE_OUTBOUND/tmp") || return 0 unless -e "$FE_OUTBOUND/tmp";
	&xunlink($inb_file) || return 0 if -e $inb_file;
	&xcplink("$INBOUND/$INBFILE",$inb_file) || return 0;

	$SIZE=(stat($inb_file))[7];
	$INSTAT_FILES{$AREA}++;
	$INSTAT_BYTES{$AREA}+=$SIZE;
	
	@linked=split(/\s+/,$SUBSCRIPTION{$AREA});

	foreach $link (keys(%PASSWORDS))
	{
		if (grep($link eq $_,@linked))
		{	
			$outb_dir="$FE_OUTBOUND/".&ftn2domain($link);
			$outb_file="$outb_dir/$FILE";
			unless (&seen($link))
			{
				&syslog('info',"Forwarding to $link");
				
				&xmkdir($outb_dir) || next unless -e $outb_dir;
				if ( -e $outb_file )
				{
					foreach $tic (<$outb_dir/*.[tT][iI][cC]>)
					{
						&xopen(T,"+<".$tic) || next;
						&xlock(T,&F_WRLCK) || next;
						while (<T>)
						{
							if (/^File[:\s]\s*(\S+)/i && $1 eq $FILE)
							{
								&xunlink($tic);
								&syslog('debug',"unlinked old TIC ".&basename($tic)." for $FILE");
								last;
							}
						}
						close(T);
					}
					&xunlink($outb_file) || next;
				}
				&xlink($inb_file,$outb_file) || next;
	
				$tic=&make_tic($link) || next;

				&syslog('debug',"Forwarded with TIC ".&basename($tic));

				$OUTSTAT_FILES{$link}++;
				$OUTSTAT_BYTES{$link}+=$SIZE;
			}
			else
			{
				&syslog('debug',"Already seen by $link");
			}
		}
		elsif ($FILEBASE && $ANNOUNCE{$link})
		{
			&xopen(F,$MAIL_COMMAND) || next;
			
			print F "From: ".$ROBOT_NAME."@".&ftn2fqdn($ADDRESS)."\n";
			print F "To: sysop@".&ftn2fqdn($link)."\n";
			print F "Subject: New file at $ADDRESS\n\n";
			$koidesc=&xcode(defined($DESC)?$DESC:$DEFAULT_DESC,@INTAB);
			print F <<EOF;
Dear sysop,
$ADDRESS received a new file $FILE in area $AREA with description:
$koidesc
To receive it use '%SEND $FILE' command.
				SY, filefix of $ADDRESS
EOF
			close(F);
		}
	}	
	&add_to_base() if defined($FILEBASE);
	&xunlink($inb_file);
	&xunlink("$INBOUND/$INBFILE");
}

sub read_tic
{
	local($tic_name)=@_;
	&xopen(TIC,$tic_name) || return 0;

	undef $AREA;
	undef $FILE;
	undef $DESC;
	undef $FROM;
	undef $TO;
	undef $REPLACES;
	undef $CRC;
	undef $ORIGIN;
	undef $PW;
	undef @PATH;
	undef @SEENBY;
	undef @UNIDENTIFIED;
	
	while (<TIC>)
	{
		$AREA=$1, next if /^Area[:\s]\s*(\S+)/i;
		$FILE=$1, next if /^File[:\s]\s*(\S+)/i;
		(defined($DESC) || ($DESC=$1)), next if /^Desc[:\s]\s*(\S.*\S)/i;
		$FROM=&canonize($1), next if m|^From[:\s]\s*(\d+:\d+/\d+(\.\d+)?)|i;
		$TO=&canonize($1), next if m|^To[:\s].*(\d+:\d+/\d+(\.\d+)?)|i;
		$REPLACES=$1, next if /^Replaces[:\s]\s*(\S+)/i;
		next if /^Magic[:\s]/i;
		next if /^Created\s+/i;
		$CRC=$1, next if /^Crc[:\s]\s*([0-9A-F]+)/i;
		$PW=&upcase($1), next if /^Pw[:\s]\s*(\S+)/i;
		$ORIGIN=$1, next if /^Origin[:\s]\s*(\S+)/i;
		push(@PATH,$1), next if /^Path[:\s]\s*(\S.*\S)/i;
		push(@SEENBY,$1), next if /^Seenby[:\s]\s*(\S.*\S)/i;
		s/[\n\r]+$/\r\n/;
		push(@UNIDENTIFIED,$_);
	}	
	close(TIC);
	return 1;
}

sub make_path
{
	local($t,@lt);
	$t=time();
	@lt=localtime($t);
	push(@PATH,$ADDRESS." ".$t.sprintf(" %02d%02d%02d.%02d%02d UTC%+05d",$lt[5],$lt[4],$lt[3],$lt[2],$lt[1],&gmtoff()));
}

sub check_tic
{
	local($tic_name)=&basename($_[0]);
	
	&syslog('warning',"No File: in TICK $tic_name"), return 0 unless defined($FILE);
	&syslog('warning',"No Area: in TICK $tic_name"), return 0 unless defined($AREA);
	&syslog('warning',"No From: in TICK $tic_name"), return 0 unless defined($FROM);
	&syslog('warning',"No Origin: in TICK $tic_name"), return 0 unless defined($ORIGIN);
	&syslog('warning',"No Crc: in TICK $tic_name"), return 0 unless defined($CRC);
	&syslog('warning',"No Pw: in TICK $tick_name"), return 0 unless defined($PW);
	return 1;
}

sub make_tic
{
	local($node,$tic_name,$TIC,$s)=@_;
	
	$tic_name="$FE_OUTBOUND/".&ftn2domain($_[0]).sprintf("/%08x.tic",&sequencer());
	&xopen(TIC,">".$tic_name) || return undef;

	print TIC "Area $AREA\r\n";
	print TIC "Areadesc $AREADESC\r\n" if defined($AREADESC);
	print TIC "File $FILE\r\n";
	print TIC "Replaces $REPLACES\r\n" if defined($REPLACES);
	print TIC "Desc $DESC\r\n" if defined ($DESC);
	print TIC "Origin $ORIGIN\r\n";
	print TIC "From ".$ADDRESS."\r\n";
	#print TIC "To ".&add_domain($node)."\r\n";
	print TIC "Crc $CRC\r\n";
	print TIC "Created by U-Tic v$VERSION (C) Yar Tikhiy (2:5020/118)\r\n";
	
	print TIC @UNIDENTIFIED;
	
	foreach $s (@PATH)
	{
		print TIC "Path $s\r\n";
	}
	foreach $s (sort(@NEW_SEENBY))
	{
		print TIC "Seenby $s\r\n";
	}
	print TIC "Pw $PASSWORDS{$node}\r\n";
	close(TIC);
	return $tic_name;
}
	
sub make_seenby
{
	local($link);
	@NEW_SEENBY=@SEENBY;
	foreach $link (split(/\s+/,$SUBSCRIPTION{$AREA}))
	{
		push(@NEW_SEENBY,$link) unless &seen($link);
	}
}

sub add_to_base
{
	local($dir,$file,$tfile,%base,$n)=("$FILEBASE/$AREA","$FILEBASE/$AREA/$FILE");
	
	&xmkdir($dir) || return 0 unless -e $dir;
	&xunlink($file) || return 0 if -e $file;
	if (defined($REPLACES))
	{
		$REPLACES=~tr/A-Z/a-z/;
		foreach $tfile (<$dir/$REPLACES>)
		{
			next unless -e $tfile;
			&xunlink($tfile) || next;
			&syslog('debug',"Replaced file ".&basename($tfile));
		}
	}

	if ((stat($FILEBASE))[0]==(stat($FE_OUTBOUND))[0])
	{
		&xlink("$FE_OUTBOUND/tmp/$FILE",$file) || return 0;
	}
	else
	{
		&xcplink("$INBOUND/$INBFILE",$file) || return 0;
	}
	chmod($FB_MODE,$file) if defined($FB_MODE);

	$DBM_BASE{$FILE}=$AREA;
				
	&xopen(F,"+>>$dir/files.bbs") || return 0;
	&xlock(F,&F_WRLCK) || (close(F),return 0);
	seek(F,0,&SEEK_SET);
	$n=0;
	while (<F>)
	{
		$n++;
		$base{$1}=$2, next if /^(\S+)\s+(\S.*\S)/;
		&syslog('err',"Bad line $n in $dir/files.bbs");
	}

	$base{$FILE}=&xcode(defined($DESC)?$DESC:$DEFAULT_DESC,@INTAB);
	seek(F,0,&SEEK_SET);
	truncate(F,0);
	foreach $n (keys(%base))
	{
		printf F "%-16s%s\n",$n,$base{$n};
	}
	close(F);
	&syslog('debug',"File $FILE has been added to the filebase");

	if ($ANNOUNCE_COMMAND)
	{
		unless ($INEWS_OPEN)
		{
			&xopen(INEWS,$ANNOUNCE_COMMAND) || die;
			$INEWS_OPEN=1;
			print INEWS "File\t\tSize(bytes)\tArea\t\tDescription\n";
		}
		printf INEWS "%-16s%-16d%-16s%s\n",$FILE,$SIZE,$AREA,$base{$FILE}; 
	}

	return 1;
}

sub move_bad
{
	local($bad_dir)=("$FE_OUTBOUND/bad");
	&xmkdir($bad_dir) || return 0 unless -e $bad_dir;
	&xcprename("$INBOUND/$INBFILE","$bad_dir/$FILE");
	&xcprename($_[0],"$bad_dir/".&basename($_[0]));
	return 1;
}

sub check_pw
{
	local($tic_name)=&basename($_[0]);
	if ($PASSWORDS{$FROM}=~/^$PW$/i)
	{
		return 1;
	}
	else
	{
		&syslog('notice',"$FROM gave incorrect password $PW for $tic_name");
		return 0;
	}
}

sub check_crc
{
	return 1 unless defined($CRC) && defined($EXT_CRC);
	if (hex($CRC)==hex(`$EXT_CRC<'$INBOUND/$INBFILE'`))
	{
		return 1;
	}
	else
	{
		&syslog('notice',"Bad CRC of file $FILE, TICK ".&basename($_[0])); 
		return 0;
	}
}

sub check_area
{
	local($tic)=&basename($_[0]);
		
	if (grep($_ eq $AREA,@AREAS))
	{
		return 1;
	}
	elsif ($ALLOW_CREATE{$FROM})
	{
		push(@AREAS,$AREA);
		&xopen(F,">>".$AREAS) || return 0;
		&xlock(F,&F_WRLCK) || (close(F), return 0);
		print F "$AREA\n";
		close(F);
		undef(%FEEDS);
		&read_feeds();
		&build_subscription();
		&syslog('notice',"Area $AREA has been autocreated");
		return 1;
	}
	else
	{
		&syslog('notice',"Unknown area $AREA in TICK $tic");
		return 0;
	}
}

sub seen
{
	local($node,$seenby)=@_;
	foreach $seenby (@SEENBY)
	{
		return 1 if &ftn_eq($node,$seenby);	
	}
	return 0;
}

sub build_subscription
{
	local(@list,$addr,$patt,$s);
	foreach $addr (keys(%PATTERNS))
	{
		next if $PASSIVE{$addr};
		foreach $patt (split(/\s+/,$PATTERNS{$addr}))
		{
			@list=();
			if ($patt=~/^!(\S+)/)
			{
				&wildmat2regexp($s=$1);
				@list=grep(!/$s/,@list);
			}
			else
			{
				&wildmat2regexp($s=$patt);
				push(@list,grep(/$s/,@AREAS));
			}
			
			foreach $s (@list)
			{	
				$SUBSCRIPTION{$s}.=$addr." ";
			}
		}
	}
}

sub build_subscription_fast
{
	local(@list,$addr,$patt,$pattern,$s,%areas_hash);
	%areas_hash=&scalar2assoc(@AREAS);

	while (($addr,$pattern)=each(%PATTERNS))
	{
		next if $PASSIVE{$addr};
		@list=();
		foreach $patt (split(/\s+/,$pattern))
		{
			if ($patt!~/^!/ && $patt!~/\*/)
			{
				if ($areas_hash{$patt})
				{
					push(@list,$patt);
				}
				else
				{
					#&syslog('warning',"%s subscribed to %s which is not active",$addr,$patt);
				}
			}
			elsif ($patt=~/^!(\S+)/)
			{
				&wildmat2regexp($s=$1);
				@list=grep(!/$s/,@list);
			}
			else
			{
				&wildmat2regexp($s=$patt);
				push(@list,grep(/$s/,@AREAS));
			}
		}
			
		foreach $s (@list)
		{	
			$SUBSCRIPTION{$s}.=$addr." ";
		}
	}
}

#!/usr/bin/perl
# FileFix for UN*X

push(@INC,$0=~m|^(.*)/[^/]+$|);

require "hacks.pl";

require "fcntl.ph";
require "stdio.ph";
#require "syslog.ph";
require "errno.ph";
require "syslog.pl";

require "machdep.pl";
require "xfutil.pl";
require "misc.pl";
require "semaphore.pl";
require "ftn.pl";
require "ctlfiles.pl";

&ufix();

sub ufix
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
	&openlog('u-fix','cons,pid',$LOG_FACILITY);
	#&syslog('info',"Startup");
	&read_felist();
	&read_areas();
	&read_passwd();
	&read_feeds();
	&read_restricted();
	&read_help();
	umask($UMASK);
	dbmopen(DBM_BASE,"$FILEBASE/fbdbm",0644) || die &syslog('err',"Cannot open $FILEBASE/fbdbm: $!") if $FILEBASE;
}

sub done
{
	&write_feeds() if $FEEDS_CHANGED;
	&write_passwd() if $PASSWD_CHANGED;
	dbmclose(%DBM_BASE) if $FILEBASE;
	#&syslog('info',"End");
	&closelog();
}

sub process
{
	local($message);

	foreach $message (<$QUEUE/*>)
	{
		next unless -f $message;
	
		&xopen(MESSAGE,$message) || next;
		unless (&process_header())
		{
			&xmkdir("$QUEUE/bad") || next unless -d "$QUEUE/bad";
			&xrename($message, "$QUEUE/bad/".&basename($message));
			&syslog('warning',"Bad header in message ".&basename($message));
			next;
		}

		&syslog('info',"Processing message from $FROM_NAME, $FROM_ADDR");

		&xopen(REPLY,$MAIL_COMMAND) || die;
		print REPLY "From: $ROBOT_NAME@".&ftn2fqdn($ADDRESS)."\n";
		print REPLY "To: ".(defined($REPLY_TO)?$REPLY_TO:$FROM)."\n";
		print REPLY "Subject: Reply from $ROBOT_NAME\n\n";
	
		if (&check_passwd())
		{
			&syslog('info',"$FROM_ADDR gave correct password");
			&process_body();
		}
		else
		{
			&syslog('notice',"$FROM_ADDR gave incorrect password $PW");
			&write_passwd_flame();
		}

		close(REPLY);		
		close(MESSAGE);

		&xunlink($message);
	}
}

sub check_passwd
{
	return defined($PASSWORDS{$FROM_ADDR}) && $PW eq $PASSWORDS{$FROM_ADDR};
}

sub write_passwd_flame
{
	print REPLY <<EOF;
Dear $FROM_NAME,
You have specified incorrect password.
			Yours, FileFix of $ADDRESS 
EOF
}

sub process_header
{
	undef $PW;
	undef $FROM;
	undef $FROM_ADDR;
	undef $FROM_NAME;
	undef $REPLY_TO;

	while (<MESSAGE>)
	{
		last if /^\s*$/;
		$FROM=&parse_addr($1), next if /^From:\s*(\S.*\S)\s*$/i;
		$REPLY_TO=&parse_addr($1), next if /^Reply-To:\s*(\S.*\S)\s*$/i;
		$PW=&upcase($1), next if /^Subject:\s*(\S+)\s*$/i;
	}
	return 0 unless defined($FROM);
	($FROM_NAME,$FROM_ADDR)=split(/@/,$FROM);
	$FROM_NAME=~s/[\._]/ /g;
	$FROM_ADDR=&domain2ftn($FROM_ADDR);
	return 1;	
}

sub parse_addr
{
	local($from)=@_;
	return $1 if ($from=~/^[^<]*<(\S+@\S+)>/ || $from=~/^(\S+@\S+)/);
	return undef;
}

sub process_body
{
	local($cmd);
	while(<MESSAGE>)
	{
		last if /^---/;
		next if /^\s*$/;

		$cmd=$_;
		$cmd=~s/%/%%/g;
		&syslog('info',"Processing '$cmd' command");
		last if /^%QUIT/i;
		&list(), next if /^%LIST/i;
		&query(), next if /^(%QUERY|%LINKED)/i;
		&unlinked(), next if /^%UNLINKED/i;
		&passive(), next if /^(%PASSIVE|%PAUSE)/i;
		&active(), next if /^(%ACTIVE|%RESUME)/i;
		&set_passwd($1), next if /^%PASSWD\s+(\S+)/i;
		&set_announce_mode($1), next if /^%ANNOUNCE\s+(ON|OFF)/i;
		&send_file($1), next if /^%SEND\s+(\S+)/i;
		&help(), next if /^%HELP/i;
		&bad_cmd(), next if /^%/i; 
		&unsubscribe($1), next if /^-(\S+)/;
		&subscribe($1), next if /^\+?(\S+)/;
	}
}

sub subscribe
{
	local($area,@areas,@patts)=@_;

	&syslog('info',"Processing subscribe operation");

	@patts=split(/\s+/,$PATTERNS{$FROM_ADDR});
	
	$area=~tr/a-z/A-Z/;
	&wildmat2regexp($area);
	if (@areas=grep(/$area/,@AREAS))
	{
		foreach $area (@areas)
		{
			unless (grep($area eq $_,@patts))
			{
				unless (&is_restricted($area))
				{
					&syslog('info',"Remote subcribed to $area");
					print REPLY "+$area - subscribed\n";
					push(@patts,$area);
					$FEEDS_CHANGED=1;
				}
				else
				{
					&syslog('info',"Remote attempted to subscribe to restricted area $area");
					print REPLY "+$area - $area is restricted for you\n";
				}
			}
			else
			{
				&syslog('info',"Remote already subscribed $area");
				print REPLY "+$area - already subscribed\n";
			}
		}
	}
	else
	{
		&syslog('info',"+$_[0] - no such area(s)");
		print REPLY "+$_[0] - no such area(s) here\n";
	}
	
	$PATTERNS{$FROM_ADDR}=join(' ',@patts);
	unless (grep($FROM_ADDR eq $_,@FEEDS_ADDRESSES))
	{
		push(@FEEDS_ADDRESSES,$FROM_ADDR);
		$FEEDS_COMMENT{$FROM_ADDR}="#\n";
	}
}

sub unsubscribe
{
	local($area,@areas,@patts)=@_;

	&syslog('info',"Processing unsubscribe operation");

	@patts=split(/\s+/,$PATTERNS{$FROM_ADDR});

	$area=~tr/a-z/A-Z/;
	&wildmat2regexp($area);
	if (@areas=grep(/$area/,@patts))
	{
		@patts=grep(!/$area/,@patts);
		foreach $area (@areas)
		{
			unless (&is_restricted($area))
			{
				&syslog('info',"Remote unsubcribed from $area");
				print REPLY "-$area - unsubscribed\n";
			}
			else
			{
				&syslog('info',"Remote attempted to unsubscribe from restricted area $area");
				print REPLY "-$area - $area is restricted for you\n";
				push(@patts,$area);
			}
		}
		$FEEDS_CHANGED=1;
	}
	else
	{
		&syslog('info',"-$_[0] - no match in remote subscription");
		print REPLY "-$_[0] - not subscribed\n";
	}
	
	$PATTERNS{$FROM_ADDR}=join(' ',@patts);
}

sub passive
{
	&syslog('info',"Remote requested PASSIVE status");
	unless ($PASSIVE{$FROM_ADDR})
	{
		$PASSIVE{$FROM_ADDR}=1;
		&syslog('info',"Remote made passive");
		print REPLY "%PASSIVE - made passive\n";
		$PASSWD_CHANGED=1;
	}
	else
	{
		&syslog('info',"Remote is already passive");
		print REPLY "%PASSIVE - already passive\n";
	}
}

sub active
{
	&syslog('info',"Remote requested ACTIVE status");
	if ($PASSIVE{$FROM_ADDR})
	{
		$PASSIVE{$FROM_ADDR}=0;
		&syslog('info',"Remote made active");
		print REPLY "%ACTIVE - made active\n";
		$PASSWD_CHANGED=1;
	}
	else
	{
		&syslog('info',"Remote is already active");
		print REPLY "%ACTIVE - already active\n";
	}
}

format AREALIST=
@<<<<<<<<<<<<<<<<<<<<<<<@<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
$area,			$description
.

sub query
{
	local($area,$description,$oldfh);

	&syslog('info',"Remote requested QUERY");
	print REPLY "%QUERY\n";

	$oldfh=select(REPLY);
	$~=AREALIST;
	foreach $area (sort(split(/\s+/,$PATTERNS{$FROM_ADDR})))
	{
		$description=defined($FELIST{$area})?$FELIST{$area}:$DEFAULT_DESC;
		write;
	}
	select($oldfh);
	print REPLY "\n";
}

sub list
{
	local($area,$description,$oldfh);

	&syslog('info',"Remote requested LIST");
	print REPLY "%LIST\n";

	$oldfh=select(REPLY);
	$~=AREALIST;
	foreach $area (sort(@AREAS))
	{
		$description=defined($FELIST{$area})?$FELIST{$area}:$DEFAULT_DESC;
		write;
	}
	select($oldfh);
	print REPLY "\n";
}

sub unlinked
{
	local($area,$description,$oldfh,@patts);

	&syslog('info',"Remote requested UNLINKED");
	print REPLY "%UNLINKED\n";

	@patts=split(/\s+/,$PATTERNS{$FROM_ADDR});

	$oldfh=select(REPLY);
	$~=AREALIST;
	foreach $area (sort(@AREAS))
	{
		unless (grep($area eq $_,@patts))
		{
			$description=defined($FELIST{$area})?$FELIST{$area}:$DEFAULT_DESC;
			write;
		}
	}
	select($oldfh);
	print REPLY "\n";
}

sub set_passwd
{
	local($pw)=@_;
	&syslog('info',"Remote changed password");
	print REPLY "%PASSWD $pw - password changed successfully\n";
	$PASSWORDS{$FROM_ADDR}=$pw;
	$PASSWD_CHANGED=1;
}

sub set_announce_mode
{
	local($mode)=@_;
	&syslog('info',"Remote changed announce mode to $mode");
	print REPLY "%ANNOUNCE $mode - announce mode set to $mode\n";
	$ANNOUNCE{$FROM_ADDR}=$mode=~/^ON$/i;
	$PASSWD_CHANGED=1;
}

sub send_file
{
	local($file)=@_;
	$file=~tr/A-Z/a-z/;
	if ($FILEBASE)
	{
		if (defined($DBM_BASE{$file}))
		{
			&syslog('info',"Remote requested file '$file'");
			if (&attach($FROM_ADDR,"$FILEBASE/$DBM_BASE{$file}/$file"))
			{
				print REPLY "%SEND $file - file attached\n";
				&syslog('info',"File attached OK");
			}
			else
			{
				print REPLY "%SEND $file - cannot attach (system error)\n";
				&syslog('warning',"Cannot attach file");
			}
		}
		else
		{
			&syslog('info',"Remote requested nonexistent file '$file'");
			print REPLY "%SEND $file - no such file here\n";
		}
	}
	else
	{
		&syslog('info',"Remote tried to request file");
		print REPLY "%SEND $file - filebase is not supported on $ADDRESS\n";
	}
}

sub help
{
	&syslog('info',"Remote requested HELP");
	print REPLY "%HELP -\n$HELP\n";
}

sub bad_cmd
{
	&syslog('info',"Remote issued bad command $_[0]");
	print REPLY "$_[0] - unknown command. Try %HELP\n";
}

sub is_restricted
{
	local($area,$area_patt)=@_;
	foreach $area_patt (keys(%RESTRICTED))
	{
		return 1 if $area=~/$area_patt/ && $FROM_ADDR=~/$RESTRICTED{$area_patt}/;
	}
	return 0;
}

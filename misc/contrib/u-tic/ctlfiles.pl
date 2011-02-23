sub read_config
{
	local($n);

	&xopen(CFG,$CONFIG) || die;
	&xlock(CFG,&F_RDLCK) || die;
	$n=0;

	$UMASK=022;
	$FLAVOR="f";
	$MAIL_COMMAND="|sendmail -t";
	$ROBOT_NAME="filefix-service";

	while (<CFG>)
	{
		$n++;
		next if /^\s*(#.*)?$/;

		$ADDRESS=&canonize($1), next if m|^Address\s+(\d+:\d+/\d+(\.\d+)?(@\S+)?)|i;

		$ROBOT_NAME=$1, next if /^Robot_Name\s+(\S+)/i;

		$OUTBOUND=$1, next if /^Outbound\s+(\S+)/i;

		$INBOUND=$1, next if /^Inbound\s+(\S+)/i;

		$FE_OUTBOUND=$1, next if /^FE_outbound\s+(\S+)/i;

		$AREAS=$1, next if /^Areas\s+(\S+)/i;

		$FELIST=$1, next if /^FE_List\s+(\S+)/i;

		$FEEDS=$1, next if /^Feeds\s+(\S+)/i;

		$RESTRICTED=$1, next if /^Restricted\s+(\S+)/i;

		$QUEUE=$1, next if /^Queue\s+(\S+)/i;

		$MAIL_COMMAND=$1, next if /^Mail_Command\s+(\S(.*\S)?)\s*$/i;

		$ANNOUNCE_COMMAND=$1, next if /^Announce_Command\s+(\S(.*\S)?)\s*$/i;

		$INTAB=$1, next if /^Intab\s+(\S+)/i;
	
		$INSTAT=$1, next if /^Instat\s+(\S+)/i;

		$OUTSTAT=$1, next if /^Outstat\s+(\S+)/i;
		
		$HELP=$1, next if /^Help\s+(\S+)/i;

		$UMASK=oct($1), next if /^Umask\s+(\d+)/i;

		$FB_MODE=oct($1), next if /^FileBase_Mode\s+(\d+)/i;

		$FLAVOR=$1, $FLAVOR=~tr/A-Z/a-z/, next if /^Flavor\s+([hfc])/i;

		$SEQUENCER=$1, next if /^Sequencer\s+(\S+)/i;

		$PASSWD=$1, next if /^Passwd\s+(\S+)/i;

		$LOG_FACILITY=$1, next if /^Log_Facility\s+(\S+)/i;

		$SEMAPHORE=$1, next if /^Semaphore\s+(\S+)/i;

		$DEFAULT_DESC=$1, next if /^Default_Description\s+(\S.*\S)\s*$/i;

		$FILEBASE=$1, next if /^FileBase\s+(\S+)/i;

		$EXT_CRC=$1, next if /^CRC_utility\s+(\S+)/i;

		$FULL_DOMAINS{$3}=$1, $DOMAINS{$3}=$2, next if /^Domain\s+(\S+)\s+(\S+)\s+(\d+)/i;

		print STDERR "Bad config line $n:\n$_";
	}
	close(CFG);
	die "Incomplete config file" unless 	defined($ADDRESS) &&
						defined($OUTBOUND) &&
						defined($INBOUND) &&
						defined($FE_OUTBOUND) &&
						defined($AREAS) &&
						defined($FEEDS) &&
						defined($PASSWD) &&
						defined($SEQUENCER) &&
						defined($SEMAPHORE) &&
						defined($QUEUE) &&
						defined($LOG_FACILITY);
}

sub read_felist
{
	local($n);
	
	&xopen(F,$FELIST) || die;
	&xlock(F,&F_RDLCK) || die;
	$n=0;
	while (<F>)
	{
		$n++;
		next if /^\s*(#.*)?$/;
		$FELIST{&upcase($1)}=$2, next if /^(\S+)\s+(\S.*\S)\s*$/;
		&syslog('err',"Bad line $n in $FELIST");
	}
	close(F);
}

sub read_areas
{
	local($n);

	&xopen(F,$AREAS) || die;
	&xlock(F,&F_RDLCK) || die;
	$n=0;
	while (<F>)
	{
		$n++;
		next if /^\s*(#.*)?$/;
		push(@AREAS,&upcase($1)), next if /^(\S+)/;
		&syslog('err',"Bad line $n in $AREAS");
	}
	close(F);
}
	
sub read_feeds
{
	local($addr,$flags,$n,$f,$comment);

	&xopen(F,$FEEDS) || die;
	&xlock(F,&F_RDLCK) || die;
	$n=0;

	while (<F>)
	{
		$n++;
		$comment.=$_, next if /^\s*(#.*)?$/;
 		if (/^(\S+)/)
		{
			$addr=&canonize($1);
			$FEEDS_COMMENT{$addr}=$comment;
			$comment="";
			push(@FEEDS_ADDRESSES,$addr); #to keep order
			next;
		}
		$PATTERNS{$addr}.=$1." ", next if /^\s+(\S+)/;
		&syslog('err',"Bad line $n in $FEEDS");	
	}
	$FEEDS_LAST_COMMENT=$comment;
	close(F);
}

sub write_feeds
{
	local($addr,$area);

	&xopen(F,">".$FEEDS) || die;
	&xlock(F,&F_WRLCK) || die;

	foreach $addr (@FEEDS_ADDRESSES)
	{
		print F $FEEDS_COMMENT{$addr};
		print F $addr."\n";
		foreach $area (split(/\s+/,$PATTERNS{$addr}))
		{
			print F "\t$area\n";
		}
	}
	print F $FEEDS_LAST_COMMENT;
	close(F);
}

sub read_passwd
{
	local($addr,$flags,$n,$comment);

	&xopen(F,$PASSWD) || die;
	&xlock(F,&F_RDLCK) || die;
	$n=0;
	
	while (<F>)
	{
		$n++;
		$comment.=$_, next if /^\s*(#.*)?$/;
		if (/^(\S+)\s+(\S+)(\s+)?(\S+)?/)
		{
			$PASSWORDS{$addr=&canonize($1)}=&upcase($2);
			$flags=$4;
			$PASSIVE{$addr}=$flags=~/P/i;
			$ANNOUNCE{$addr}=$flags=~/A/i;
			$ALLOW_CREATE{$addr}=$flags=~/C/i;
			$NO_FLO{$addr}=$flags=~/N/i;

			$PASSWD_COMMENT{$addr}=$comment;
			$comment="";
		
			push(@PASSWD_ADDRESSES,$addr); # to keep order
			
			next;
		}
		&syslog('err',"Bad line $n in $PASSWD");
	}
	$PASSWD_LAST_COMMENT=$comment;
	close(F);
}

sub write_passwd
{
	local($addr,$flags);

	&xopen(F,">".$PASSWD) || die;
	&xlock(F,&F_WRLCK) || die;

	foreach $addr (@PASSWD_ADDRESSES)
	{
		undef $flags;
		$flags.="P" if $PASSIVE{$addr};
		$flags.="C" if $ALLOW_CREATE{$addr};
		$flags.="A" if $ANNOUNCE{$addr};
		$flags.="N" if $NO_FLO{$addr};

		print F $PASSWD_COMMENT{$addr};
		print F "$addr\t$PASSWORDS{$addr}\t$flags\n";
	}
	print F $PASSWD_LAST_COMMENT;
	close(F);
}

sub read_restricted
{
	local($n,$area_patt,$addr_patt);

	&xopen(F,$RESTRICTED) || die;
	&xlock(F,&F_RDLCK) || die;
	$n=0;
	while (<F>)
	{
		$n++;
		next if /^\s*(#.*)?$/;
		if (($area_patt,$addr_patt)=/^(\S+)\s+(\S+)/)
		{
			&wildmat2regexp($area_patt);
			&wildmat2regexp($addr_patt);
			$RESTRICTED{$area_patt}=$addr_patt;
			next;
		}
		&syslog('err',"Bad line $n in $RESTRICTED");
	}
	close(F);
}

sub read_help
{
	&xopen(F,$HELP) || die;
	&xlock(F,&F_RDLCK) || die;
	undef $HELP;
	$HELP.=$_ while <F>;
	close(F);
}

sub read_stats
{
	if ($INSTAT && open(F,$INSTAT))
	{
		&xlock(F,&F_RDLCK) || die;
		while (<F>)
		{
			$INSTAT_FILES{$1}=$2, $INSTAT_BYTES{$1}=$3 if /^(\S+)\s+(\d+)\s+(\d+)/;
		}
		close(F);
	}
	if ($OUTSTAT && open(F,$OUTSTAT))
	{
		&xlock(F,&F_RDLCK) || die;
		while (<F>)
		{
			$OUTSTAT_FILES{$1}=$2, $OUTSTAT_BYTES{$1}=$3 if /^(\S+)\s+(\d+)\s+(\d+)/;
		}
		close(F);
	}
}

sub write_stats
{
	if ($INSTAT && open(F,">".$INSTAT))
	{
		&xlock(F,&F_WRLCK) || die;
		foreach $i (keys(%INSTAT_FILES))
		{
			print F $i."\t".$INSTAT_FILES{$i}."\t".$INSTAT_BYTES{$i}."\n";
		}
		close(F);
	}
	if ($OUTSTAT && open(F,">".$OUTSTAT))
	{
		&xlock(F,&F_WRLCK) || die;
		foreach $i (keys(%OUTSTAT_FILES))
		{
			print F $i."\t".$OUTSTAT_FILES{$i}."\t".$OUTSTAT_BYTES{$i}."\n";
		}
		close(F);
	}
}

sub read_intab
{
	if ($INTAB && &xopen(F,$INTAB))
	{
		&xlock(F,&F_RDLCK) || die;
		while (<F>)
		{
			next if /^\s*(#.*)?$/;
			$INTAB[hex($1)]=hex($2) if /^\s*0x([0-9a-f]+)\s+0x([0-9a-f]+)/i;
			$INTAB[$1]=$2 if /^\s*(\d+)\s+(\d+)/;
		}
		close(F);
	}
}

1;

#!/usr/bin/perl
# Filebox manager for U-Tic

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

&uattach();

sub uattach
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
	&openlog('u-attach','cons,pid',$LOG_FACILITY);
	#&syslog('info',"Startup");
	&read_passwd();
	umask($UMASK);
}

sub done
{
	&closelog();
}

sub process
{
	local($link,$flo,$file,$outb_dir,$outb_file,$outb,@flo,%tix);
	
	foreach $link (keys(%PASSWORDS))
	{
		$outb_dir="$FE_OUTBOUND/".&ftn2domain($link);
		next unless -d $outb_dir;
		next if $NO_FLO{$link};

		$outb=&outb_path($link);
		$flo="$outb/".&flo($link);
		&xmkdir($outb) || next unless -e $outb;

		&nodelock($link,&NL_NOWAIT) || next;
	
		undef @flo;
		undef %tix;

		&xopen(IN,"+>>$flo") || next;
		&xlock(IN,&F_WRLCK,1) || (close(IN), next);
		seek(IN,0,&SEEK_SET);
		push(@flo,$_) while (<IN>);

		foreach $outb_file (<$outb_dir/*.[tT][iI][cC]>)
		{
			next if grep($_ eq "^$outb_file\n",@flo);
			&xopen(T,$outb_file) || next;
			&xlock(T,&F_RDLCK,1) || next;
			undef $file;
			while (<T>)
			{
				$file=$1, last if /^File[:\s]?\s*(\S+)/i;
			}
			close(T);
			if ($file)
			{
				$tix{$file}=$outb_file;
			}
			else
			{
				&syslog('warning',"skipping tic w/o 'FILE' ".&basename($outb_file)." for $link");
			}
		}
		
		foreach $outb_file (<$outb_dir/*>)
		{
			next if $outb_file=~/\.[tT][iI][cC]$/;
			next if grep($_ eq "^$outb_file\n",@flo);
			push(@flo,"^$outb_file\n");
			&syslog('info',&basename($outb_file)." attached to $link");
			$file=&basename($outb_file);
			if (defined($tix{$file}))
			{
				push(@flo,"^$tix{$file}\n");
				&syslog('info',&basename($tix{$file})." attached to $link");
				delete $tix{$file};
			}
		}
	
		foreach $outb_file (values(%tix))
		{	
			push(@flo,"^$outb_file\n");
			&syslog('info',&basename($outb_file)." attached to $link");
		}

		seek(IN,0,&SEEK_SET);
		truncate(IN,0);
		print IN @flo;
		close(IN);

		&nodeunlock($link);
	}
}

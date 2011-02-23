sub semaphore_on
{
	local($name,$oldpid)="$SEMAPHORE/".&basename($0).".pid";

	if ( -e $name )
	{
		&xopen(SEM,$name) || die;
		$oldpid=<SEM>;
		close(SEM);
		die &syslog('err',"Bad PID in semaphore file $name") unless ($oldpid>0);	
		if (!kill(0,$oldpid))
		{
			&syslog('warning',"Dead semaphore (PID=$oldpid) found");
			&xunlink($name) || die;
		}
		elsif (time()-(stat($name))[9]>86400)	# 1 day
		{
			die &syslog('err',"Possibly hung process (PID=$oldpid)");
		}
		else
		{
			&syslog('warning',"Overlapping with [$oldpid] - exiting");
			exit;
		} 
	}

	open(SEM,">".$name);
	print SEM $$;
	close(SEM);
}

sub semaphore_off
{
	&xunlink("$SEMAPHORE/".&basename($0).".pid") || die; 
}

sub semlock
{
	local($name,$oldpid)=@_;
	&xopen(SEM,">$name.$$") || die;
	print SEM $$;
	close(SEM);

	while (!link("$name.$$",$name))
	{
		$!==&EEXIST || die &syslog("Someone killed my temporary file");
		if (&xopen(SEM,$name))
		{
			$oldpid=<SEM>;
			close(SEM);
			unless (kill(0,$oldpid))
			{
				&syslog('warning',"Dead lock $name (PID=$oldpid) found");
				&xunlink($name) || die;
			}
		}
	}
}

1;

sub bsy
{
	return (&point($_[0])?sprintf("%4.4x%4.4x.",0,&point($_[0])):sprintf("%04x%04x.",&net($_[0]),&node($_[0])))."bsy";
}

sub flo
{
	return (&point($_[0])?sprintf("%4.4x%4.4x.",0,&point($_[0])):sprintf("%04x%04x.",&net($_[0]),&node($_[0]))).$FLAVOR."lo";
}

sub outb_path
{
	return $OUTBOUND.(&point($_[0])?sprintf("/%04x%04x.pnt",&net($_[0]),&node($_[0])):"");
}

sub attach
{
	local($addr,$file,$dir)=@_;
	&nodelock($addr) || return 0;
	$dir=&outb_path($addr);
	&xmkdir($dir) || return 0 unless -e $dir;
	&xopen(F,">>".$dir."/".&flo($addr)) || return 0;
	&xlock(F,&F_WRLCK) || return 0;
	print F $file."\n";
	close(F);	
	&nodeunlock($addr);
	return 1;
}

sub ftn2domain
{
	local($d,$s)=@_;
	$d=~m|^(\d+):(\d+)/(\d+)|;
	$s="f$3.n$2.z$1";
	$s="p$1.$s" if $d=~/\.(\d+)/;
	return $s;
}

sub ftn2fqdn
{
	local($d,$s)=@_;
	$d=~m|^(\d+):(\d+)/(\d+)|;
	$s="f$3.n$2.z$1";
	$s.=".".$FULL_DOMAINS{$1};
	$s="p$1.$s" if $d=~/\.(\d+)/;
	return $s;
}

sub domain2ftn
{
	local($d,$s,$z)=@_;
	$d=~/f(\d+)\.n(\d+)\.z(\d+)/;
	$z=$3;
	$s="$3:$2/$1";
	$s.=".$1" if $d=~/^p(\d+)/;
	return $s;
}

sub zone
{
	return $_[0]=~m|^(\d+):|;
}

sub net
{
	return $_[0]=~m|:(\d+)/|;
}

sub node
{
	return $_[0]=~m|/(\d+)|;
}

sub point
{
	return $_[0]=~m|\.(\d+)|;
}

sub canonize
{
	local($s)=@_;
	$s=~s/@.*$//;
	$s=~s/\.0$//;
	return $s;
}

sub add_domain
{
	local($s,$z)=@_;
	$z=$s=~/^(\d+):/;
	$s.="@".$DOMAINS{$z} if defined($DOMAINS{$z});
	return $s;
}

sub ftn_eq
{
	return &canonize($_[0]) eq &canonize($_[1]);
}

sub NL_WAIT {0;}
sub NL_NOWAIT {1;}

sub nodelock
{
	local($link,$mode,$outb,$bsy,$c)=@_;
	$outb=&outb_path($link);
	$bsy="$outb/".&bsy($link);
	&xmkdir($outb) || next unless -e $outb;

	&xopen(BSY,">$bsy.$$") || return 0;
	print BSY $$;
	close(BSY);

	$c=0;
	$rc=1;

	while (!link("$bsy.$$",$bsy))
	{
		unless ($! == &EEXIST)
		{
			&syslog('err',"link($bsy.$$,$bsy): $!");
			$rc=0;
			last;
		}

		$rc=0, last if $c>0 && $mode==&NL_NOWAIT; 

		unless (open(BSY,$bsy))
		{
			if ($! == &ENOENT)
			{
				next;
			}
			else
			{
				&syslog('err',"open($bsy): $!");
				$rc=0;
				last;
			}
		}
		$pid=<BSY>;
		close(BSY);
		unless ($pid>0 && kill(0,$pid))
		{
			unless (unlink($bsy) || $!==&ENOENT)
			{
				&syslog('err',"unlink($bsy): $!");
				$rc=0;
				last;
			}
		}
		elsif ($mode==&NL_NOWAIT)
		{
			$rc=0;
			last;
		}
		else
		{
			sleep(10);
		}
		
		$c++;
	}
	&xunlink("$bsy.$$");
	return $rc;
}

sub nodeunlock
{
	local($link,$bsy,$pid)=@_;
	$outb=&outb_path($link);
	$bsy="$outb/".&bsy($link);

	&xopen(BSY,$bsy) || return 0;
	$pid=<BSY>;
	close(BSY);
	if (!$pid)
	{
		&syslog('warning',"Bad PID in bsy '$bsy'");
		return 0;
	}
	elsif ($pid==$$)
	{
		&xunlink($bsy);
		return 1;
	}
	else
	{
		&syslog('warning',"Trying to remove bsy for PID $pid");
		return 0;
	}
}

1;

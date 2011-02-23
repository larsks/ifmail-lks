sub xlog
{
	&syslog($_[0],$_[1]);
	return 0;
}

sub xopen
{
	return open($_[0],$_[1]) || &xlog('err',"open($_[1]): $!");
}

sub xmkdir
{
	return mkdir($_[0],0777) || &xlog('err',"mkdir($_[0]): $!");
}

sub xrename
{
	return rename($_[0],$_[1]) || &xlog('err',"rename($_[0],$_[1]): $!");
}

sub xunlink
{
	return unlink($_[0]) || &xlog('err',"unlink($_[0]): $!");
}

sub xlock
{
	return &f_lock($_[0],$_[1],$_[2]) || &xlog('err',"fcntl(): $!");
}

sub xlink
{
	return link($_[0],$_[1]) || &xlog('err',"link($_[0],$_[1]): $!");
}

sub xcplink
{
	local(@st1,@st2,$lndir);
	($lndir)=$_[1]=~m|^(.*)/[^/]+$|;
	if ((@st1=stat($_[0])) && (@st2=stat($lndir)))
	{
		if ($st1[0]==$st2[0])	#device ID
		{
			return link($_[0],$_[1]) || &xlog('err',"link($_[0],$_[1]): $!");
		}
		else
		{
			return &copy($_[1],$_[0]);
		}
	}
	else
	{
		return 0;
	}
}

sub xcprename
{
	local(@st1,@st2,$lndir);
	($lndir)=$_[1]=~m|^(.*)/[^/]+$|;
	if ((@st1=stat($_[0])) && (@st2=stat($lndir)))
	{
		if ($st1[0]==$st2[0])	#device ID
		{
			return rename($_[0],$_[1]) || &xlog('err',"rename($_[0],$_[1]): $!");
		}
		else
		{
			return &copy($_[1],$_[0]) && &xunlink($_[0]);
		}
	}
	else
	{
		return 0;
	}
}

sub copy
{
	local($c,$buf);
	&xopen(SRC,$_[1]) || return 0;
	&xlock(SRC,&F_RDLCK) || (close(SRC), return 0);
	&xopen(DEST,">".$_[0]) || (close(SRC), return 0);
	&xlock(DEST,&F_WRLCK) || (close(SRC), close(DEST), return 0);
	print DEST $buf while read(SRC,$buf,65536);
	close(SRC);
	close(DEST);
	return 1;
}

1;

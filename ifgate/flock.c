#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "lutil.h"

int f_lock(fn)
char *fn;
{
	int lfd=-1;
	struct flock fl;
	struct stat st;

	if (fn)
	{
		if ((lfd=open(fn,O_RDWR | O_CREAT)) < 0)
		{
			logerr("$Error opening file %s",fn);
			return -1;
		}
		fl.l_type=F_WRLCK;
		fl.l_whence=0;
		fl.l_start=0L;
		fl.l_len=0L;
		fl.l_pid=getpid();
		if (fcntl(lfd,F_SETLK,&fl) != 0)
		{
			if (errno != EAGAIN)
				loginf("$Error locking file %s",fn);
			close(lfd);
			return -1;
		}
		if (stat(fn,&st) != 0)
		{
			logerr("$Error accessing file %s",fn);
			close(lfd);
			return -1;
		}
	}
	return lfd;
}

void funlock(fd)
int fd;
{
/*
	struct flock fl;

	fl.l_type=F_UNLCK;
	fl.l_whence=0;
	fl.l_start=0L;
	fl.l_len=0L;
	fl.l_pid=getpid();
	if (fcntl(fd,F_SETLK,&fl) != 0)
	{
		logerr("$Error ulocking fd %d",fd);
	}
*/
	close(fd);
	return;
}

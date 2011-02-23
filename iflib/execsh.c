#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include "lutil.h"

#define SHELL "/bin/sh"

int execsh(cmd,in,out,err)
char *cmd,*in,*out,*err;
{
	int pid,status,rc,sverr;

	loginf("Execute command: %s",S(cmd));
	fflush(stdout);
	fflush(stderr);
	if ((pid=fork()) == 0)
	{
		if (in)
		{
			close(0);
			if (open(in,O_RDONLY) != 0)
			{
				logerr("$Reopen of stdin to %s failed",S(in));
				exit(-1);
			}
		}
		if (out)
		{
			close(1);
			if (open(out,O_WRONLY | O_APPEND | O_CREAT,0600) != 1)
			{
				logerr("$Reopen of stdout to %s failed",S(out));
				exit(-1);
			}
		}
		if (err)
		{
			close(2);
			if (open(err,O_WRONLY | O_APPEND | O_CREAT,0600) != 2)
			{
				logerr("$Reopen of stderr to %s failed",S(err));
				exit(-1);
			}
		}
		rc=execl(SHELL,"sh","-c",cmd,NULL);
		logerr("$Exec \"%s\" returned %d",S(cmd),rc);
		exit(-1);
	}
	do
	{
		rc=wait(&status);
		sverr=errno;
		debug(2,"$Wait returned %d, status %d,%d",
			rc,status>>8,status&0xff);
	}
	while (((rc > 0) && (rc != pid)) || ((rc == -1) && (sverr == EINTR)));
	if (rc == -1)
	{
		logerr("$Wait returned %d, status %d,%d",
			rc,status>>8,status&0xff);
		return -1;
	}
	debug(2,"status=%d",status);
	return status;
}

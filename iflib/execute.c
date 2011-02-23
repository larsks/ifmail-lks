#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include "lutil.h"

int execute(cmd,file,pkt,in,out,err)
char *cmd,*file,*pkt,*in,*out,*err;
{
	char buf[512];
	char *vector[16];
	int i;
	char *p,*q,*f=file,*k=pkt;
	int pid,status,rc,sverr;

	for (p=cmd,q=buf;(*p) && (q < (buf+sizeof(buf)-1));p++)
	switch (*p)
	{
	case '$':	switch (*(++p))
			{
			case 'f':
			case 'F':
				if (f)
				while ((*f) && (q < (buf+sizeof(buf)-1)))
					*(q++)=*(f++);
				f=file;
				break;
			case 'p':
			case 'P':
				if (k)
				while ((*k) && (q < (buf+sizeof(buf)-1)))
					*(q++)=*(k++);
				k=pkt;
				break;
			default:
				*(q++)='$';
				*(q++)=*p;
				break;
			}
			break;
	case '\\':	*(q++)=*(++p); break;
	default: *(q++)=*p; break;
	}
	*q='\0';
	loginf("Execute: %s",buf);
	i=0;
	vector[i++]=strtok(buf," \t\n");
	while ((vector[i++]=strtok(NULL," \t\n")) && (i<16));
	vector[15]=NULL;
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
		rc=execv(vector[0],vector);
		logerr("$Exec \"%s\" returned %d",S(vector[0]),rc);
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

#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include "lutil.h"

static struct _fppid {
	FILE *fp;
	int pid;
} fppid[] = {
	{NULL,0},
	{NULL,0},
	{NULL,0}
};
#define maxfppid 2

FILE *expipe(cmd,from,to)
char *cmd,*from,*to;
{
	char buf[256],*buflimit;
	char *vector[16];
	int i,rc;
	char *p,*q,*f=from,*t=to;
	int pipedes[2];
	FILE *fp;
	int pid,slot;

	buflimit=buf+sizeof(buf)-1-
		(f&&t&&(strlen(f)>strlen(t))?strlen(f):t?strlen(t):0);

	for (slot=0;slot<=maxfppid;slot++) {
		if (fppid[slot].fp == NULL) break;
	}
	if (slot > maxfppid) {
		logerr("attemt to pipe more than %d processes",maxfppid + 1);
		return NULL;
	}

	for (p=cmd,q=buf;(*p);p++) {
		if (q > buflimit) {
			logerr("attemt to pipe too long command");
			return NULL;
		}
		switch (*p) {
		case '$':	switch (*(++p))
				{
				case 'f':
				case 'F': if ((f)) while (*f) *(q++)=*(f++);
					f=from; break;
				case 't':
				case 'T': if ((t)) while (*t) *(q++)=*(t++);
					t=to; break;
				default: *(q++)='$'; *(q++)=*p; break;
				}
				break;
		case '\\':	*(q++)=*(++p); break;
		default: *(q++)=*p; break;
		}
	}
	*q='\0';
	loginf("Expipe: %s",buf);
	i=0;
	vector[i++]=strtok(buf," \t\n");
	while ((vector[i++]=strtok(NULL," \t\n")) && (i<16));
	vector[15]=NULL;
	fflush(stdout);
	fflush(stderr);
	if (pipe(pipedes) != 0) {
		logerr("$pipe failed for command \"%s\"",S(vector[0]));
		return NULL;
	}
	debug(2,"pipe() returned %d,%d",pipedes[0],pipedes[1]);
	if ((pid=fork()) == 0) {
		close(pipedes[1]);
		close(0);
		if (dup(pipedes[0]) != 0) {
			logerr("$Reopen of stdin for command %s failed",
				S(vector[0]));
			exit(-1);
		}
		rc=execv(vector[0],vector);
		logerr("$Exec \"%s\" returned %d",S(vector[0]),rc);
		exit(-1);
	}
	close(pipedes[0]);
	if ((fp=fdopen(pipedes[1],"w")) == NULL) {
		logerr("$fdopen failed for pipe to command \"%s\"",
			S(vector[0]));
	}
	fppid[slot].fp=fp;
	fppid[slot].pid=pid;
	return fp;
}

int exclose(fp)
FILE *fp;
{
	int status,rc;
	int pid,slot,sverr;

	for (slot=0;slot<=maxfppid;slot++) {
		if (fppid[slot].fp == fp) break;
	}
	if (slot > maxfppid) {
		logerr("attempt to close unopened pipe");
		return -1;
	}
	pid=fppid[slot].pid;
	fppid[slot].fp=NULL;
	fppid[slot].pid=0;

	debug(2,"closing pipe to the child process %d",pid);
	if ((rc=fclose(fp)) != 0) {
		logerr("$error closing pipe to transport (rc=%d)",rc);
		if ((rc=kill(pid,SIGKILL)) != 0)
			logerr("$kill for pid %d returned %d",pid,rc);
	}
	debug(2,"waiting for process %d to finish",pid);
	do {
		rc=wait(&status);
		sverr=errno;
		debug(2,"$Wait returned %d, status %d,%d",
			rc,status>>8,status&0xff);
	} while (((rc > 0) && (rc != pid)) || ((rc == -1) && (sverr == EINTR)));
	if (rc == -1) {
		logerr("$Wait returned %d, status %d,%d",
			rc,status>>8,status&0xff);
		return -1;
	}
	debug(2,"status=%d",status);
	return status;
}

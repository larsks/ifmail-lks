#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include "xutil.h"
#include "lutil.h"
#include "ftn.h"

#ifdef NEED_BSY

#if defined(SHORT_PID_T)
#define pid_t short
#elif defined(INT_PID_T)
#define pid_t int
#endif

extern char *bsyname(faddr *);
extern void mkdirs(char*);

int nodelock(addr)
faddr *addr;
{
	char *fn,*tfn,*p;
	char tmp[16];
	FILE *fp;
	pid_t pid,mypid;
	int tmppid,sverr;

	debug(4,"try locking node %s",ascfnode(addr,0x1f));

	fn=bsyname(addr);
	tfn=xstrcpy(fn);
	if ((p=strrchr(tfn,'/'))) *++p='\0';
	mypid=getpid();
	sprintf(tmp,"aa%d",mypid);
	tfn=xstrcat(tfn,tmp);
	mkdirs(tfn);
	if ((fp=fopen(tfn,"w")) == NULL) {
		logerr("$cannot open tmp file for bsy lock \"%s\"",tfn);
		free(tfn);
		return 1;
	}
	fprintf(fp,"%10d\n",mypid);
	fclose(fp);
	chmod(tfn,0444);
	if (link(tfn,fn) == 0) {
		debug(4,"created lock OK");
		unlink(tfn);
		free(tfn);
		return 0;
	} else {
		sverr=errno;
		debug(4,".bsy file present, check staleness");
	}

	if (sverr != EEXIST) {
		logerr("$could not link \"%s\" to \"%s\"",tfn,fn);
		unlink(tfn);
		free(tfn);
		return 1;
	}

	if ((fp=fopen(fn,"r")) == NULL) {
		logerr("$could not open existing lock file \"%s\"",fn);
		unlink(tfn);
		free(tfn);
		return 1;
	}

	fscanf(fp,"%d",&tmppid);
	pid=tmppid;
	fclose(fp);
	debug(4,"opened bsy file for pid %d",pid);
	if (kill(pid,0) && (errno == ESRCH)) {
		loginf("found stale bsy file for %s, unlink",
			ascfnode(addr,0x1f));
		unlink(fn);
	} else {
		debug(4,"process active, lock failed");
		unlink(tfn);
		free(tfn);
		return 1;
	}

	if (link(tfn,fn) == 0) {
		debug(4,"created lock OK");
		unlink(tfn);
		free(tfn);
		return 0;
	} else {
		logerr("$could not link \"%s\" to \"%s\"",tfn,fn);
		unlink(tfn);
		free(tfn);
		return 1;
	}
}

int nodeulock(addr)
faddr *addr;
{
	char *fn;
	FILE *fp;
	pid_t pid,mypid;
	int tmppid;

	debug(4,"try unlocking node %s",ascfnode(addr,0x1f));

	fn=bsyname(addr);
	if ((fp=fopen(fn,"r")) == NULL) {
		logerr("$cannot open lock file \"%s\"",fn);
		return 1;
	}
	fscanf(fp,"%d",&tmppid);
	pid=tmppid;
	fclose(fp);
	mypid=getpid();

	if (pid == mypid) {
		debug(4,"unlinking lock file");
		unlink(fn);
		return 0;
	} else {
		logerr("trying unlock file for process %u, we are %u",
			pid,mypid);
		return 1;
	}
}

#else /* don't NEED_BSY */

int nodelock(addr)
faddr *addr;
{
	debug(4,"try locking node %s - no locking",ascfnode(addr,0x1f));
	return 0;
}

int nodeulock(addr)
faddr *addr;
{
	debug(4,"try unlocking node %s - no locking",ascfnode(addr,0x1f));
	return 0;
}

#endif /* NEED_BSY */

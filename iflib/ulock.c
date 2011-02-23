#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <signal.h>
#include <sys/stat.h>
#include "lutil.h"

#ifndef LOCKDIR
#define LOCKDIR "/usr/spool/uucp"
#endif

#define LCKPREFIX LOCKDIR"/LCK.."
#define LCKTMP LOCKDIR"/TMP."

#ifdef DONT_HAVE_PID_T
#define pid_t int
#endif

int lock(line)
char *line;
{
	pid_t	mypid,rempid=0;
	int	tmppid;
	char	tmpname[256],lckname[256];
	char	*p;
	int	i,rc;
	FILE	*f;

	rc=-1;
	if ((p=strrchr(line,'/')) == NULL) p=line; else p++;
	mypid=getpid();
	sprintf(tmpname,"%s%d",LCKTMP,mypid);
	if ((f=fopen(tmpname,"w")) == NULL)
	{
		debug(4,"lock cannot create %s",tmpname);
		return(-1);
	}
#if defined(ASCII_LOCKFILES)
	fprintf(f,"%10d\n",mypid);
#elif defined(BINARY_LOCKFILES)
	fwrite(&mypid,sizeof(mypid),1,f);
#else
#error Must define ASCII_LOCKFILES or BINARY_LOCKFILES
#endif
	fclose(f);
	chmod(tmpname,0444);
	sprintf(lckname,"%s%s",LCKPREFIX,p);
	p=lckname+strlen(lckname)-1;
	*p=tolower(*p);
	debug(4,"Trying to create %s for %d",lckname,mypid);
	for (i=0; (i++<5) && ((rc=link(tmpname,lckname)) != 0) && 
				(errno == EEXIST); )
	{
		if ((f=fopen(lckname,"r")) == NULL)
		{
			debug(4,"cannot open existing lock file");
		}
		else
		{
#if defined(ASCII_LOCKFILES)
			fscanf(f,"%d",&tmppid);
			rempid=tmppid;
#elif defined(BINARY_LOCKFILES)
			fread(&rempid,sizeof(rempid),1,f);
#endif
			fclose(f);
			debug(4,"lock file read for process %d",rempid);
		}
		if (kill(rempid,0) && (errno ==  ESRCH))
		{
			debug(4,"process inactive, unlink file");
			unlink(lckname);
		}
		else
		{
			debug(4,"process active, sleep a bit");
			sleep(2);
		}
	}
	unlink(tmpname);
	debug(4,"lock result %d (errno %d)",rc,errno);
	return(rc);
}

int ulock(line)
char *line;
{
	pid_t	mypid,rempid;
	int	tmppid;
	char	lckname[256];
	char	*p;
	int	rc;
	FILE	*f;

	rc=-1;
	if ((p=strrchr(line,'/')) == NULL) p=line; else p++;
	mypid=getpid();
	sprintf(lckname,"%s%s",LCKPREFIX,p);
	p=lckname+strlen(lckname)-1;
	*p=tolower(*p);
	if ((f=fopen(lckname,"r")) == NULL)
	{
		logerr("$cannot open lock file %s",lckname);
		return rc;
	}
#if defined(ASCII_LOCKFILES)
	fscanf(f,"%d",&tmppid);
	rempid=tmppid;
#elif defined(BINARY_LOCKFILES)
	fread(&rempid,sizeof(rempid),1,f);
#endif
	fclose(f);
	if (rempid ==  mypid)
		rc=unlink(lckname);
	return(rc);
}

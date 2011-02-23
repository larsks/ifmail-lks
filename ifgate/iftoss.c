#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#ifdef HAS_SYSLOG
#include <syslog.h>
#endif
#include "mygetopt.h"
#include "lutil.h"
#include "config.h"
#include "version.h"
#include "ftn.h"
#include "getheader.h"
#include "trap.h"

extern int getmessage(FILE *,faddr *,faddr *);
extern void readareas(char *);
extern void readalias(char *);
extern int exclose(FILE *);

extern int num_echo,num_mail;

int usetmp=1; /* to tell bgets that we do not use batch mode */
int notransports=0;

void usage(name)
char *name;
{
#ifdef RELAXED
	confusage("-N");
	fprintf(stderr,"-N		put messages to /tmp/ifmail directory\n");
#else
	confusage("-N -f");
	fprintf(stderr,"-N		put messages to /tmp/ifmail directory\n");
	fprintf(stderr,"-f		force tossing of packets addressed to other nodes\n");
#endif
}

FILE *nb = NULL;

int main(argc,argv)
int argc;
char *argv[];
{
	int c;
	int rc,maxrc;
#ifdef RELAXED
	int relaxed=1;
#else
	int relaxed=0;
#endif
	faddr from,to;

#if defined(HAS_SYSLOG) && defined(MAILLOG)
	logfacility=MAILLOG;
#endif

	setmyname(argv[0]);
	catch(myname);
#ifdef RELAXED
	while ((c=getopt(argc,argv,"Nhx:I:")) != -1)
#else
	while ((c=getopt(argc,argv,"Nfhx:I:")) != -1)
#endif
	if (confopt(c,optarg)) switch (c)
	{
		case 'N':	notransports=1; break;
#ifndef RELAXED
		case 'f':	relaxed=1; break;
#endif
		default:	usage(argv[0]); exit(1);
	}

	if (readconfig())
	{
		logerr("Error getting configuration, aborting");
		exit(1);
	}

	readareas(areafile);
	readalias(aliasfile);

	if (notransports)
	{
		mkdir("/tmp/ifmail",0777);
		loginf("messages/newsbatches will go to /tmp/ifmail");
	}

#ifdef PARANOID
	if (((rc=getheader(&from,&to,stdin)) != 0) &&
	    ((rc != 3) || (!relaxed)))
#else
	if (((rc=getheader(&from,&to,stdin)) != 0) &&
	    ((rc != 3) || (!relaxed)) &&
	    (rc != 4))
#endif
	{
		logerr("%s, aborting",(rc==3)?"packet not to this node":
			(rc==4)?"bad password":"bad packet");
		exit(rc);
	}

	while ((rc=getmessage(stdin,&from,&to)) == 1);

	maxrc=rc;

	if (nb)
	{
		if (notransports) rc=fclose(nb);
		else rc=exclose(nb);
		if (rc < 0) rc=10-rc;
		if (rc > maxrc) maxrc=rc;
	}

	loginf("end %d echomail, %d netmail messages processed, rc=%d",
		num_echo,num_mail,maxrc);

	return maxrc;
}

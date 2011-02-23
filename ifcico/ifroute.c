#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAS_SYSLOG
#include <syslog.h>
#endif
#include <sysexits.h>
#include "mygetopt.h"
#include "lutil.h"
#include "xutil.h"
#include "ftn.h"
#include "nodelist.h"
#include "config.h"
#include "version.h"
#include "trap.h"

void usage(void)
{
	confusage("<node>");
	fprintf(stderr,"  <node>	in domain form, e.g. f11.n22.z3\n");
}

faddr *bestroute(faddr*,faddr*,node*);

int main(argc,argv)
int argc;
char *argv[];
{
	int c,rc;
	faddr *adr=NULL,*raddr=NULL;
	faddr *bestaka;
	node *nlent;

#if defined(HAS_SYSLOG) && defined(MAILLOG)
        logfacility=MAILLOG;
#endif

	setmyname(argv[0]);
	catch(myname);
	while ((c=getopt(argc,argv,"x:r:n:l:a:I:h")) != -1)
	if (confopt(c,optarg)) switch (c)
	{
		default:	usage(); exit(EX_USAGE);
	}

	if (readconfig())
	{
		fprintf(stderr,"Error getting configuration, aborting\n");
		exit(EX_DATAERR);
	}

	if (argv[optind] == NULL)
	{
		usage();
		rc=EX_USAGE;
	}
	else if ((adr=parsefaddr(argv[optind])) == NULL)
	{
		logerr("unrecognizable address \"%s\"",argv[optind]);
		rc=EX_USAGE;
	}
	else
	{
		bestaka=bestaka_s(adr);
		nlent=getnlent(adr);
		if (nlent->pflag != NL_DUMMY)
		{
			raddr=bestroute(adr,bestaka,nlent);
			printf("%s\n",ascinode(raddr,0x3f));
			rc=0;
		}
		else rc=EX_NOHOST;
	}

	return rc;
}

faddr *bestroute(remote,local,nlent)
faddr *remote,*local;
node *nlent;
{
	return remote;
}

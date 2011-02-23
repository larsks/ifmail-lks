#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef HAS_SYSLOG
#include <syslog.h>
#endif
#include "mygetopt.h"
#include "lutil.h"
#include "nodelist.h"
#include "nlindex.h"
#include "config.h"
#include "version.h"
#include "trap.h"

extern int nodebld(void);

void usage(void)
{
	confusage("");
}

int main(argc,argv)
int argc;
char *argv[];
{
	int c,rc;
	char buf[64];

#if defined(HAS_SYSLOG) && defined(MAILLOG)
	logfacility=MAILLOG;
#endif

	setmyname(argv[0]);
	catch(myname);
	while ((c=getopt(argc,argv,"x:I:h")) != -1)
	if (confopt(c,optarg)) switch (c)
	{
		default:	usage(); exit(1);
	}

	if ((rc=readconfig()))
	{
		fprintf(stderr,"Error getting configuration, aborting\n");
		return rc;
	}

	rc=initnl();

	if ((rc == 0) && (isatty(0)))
	{
		printf("Nodelist index is up to date.\n");
		printf("Do you really want to rebuild it [y/N] ? ");
		fflush(stdout);
		fgets(buf,sizeof(buf)-1,stdin);
		if ((buf[0] == 'y') || (buf[0] == 'Y'))
		{
			rc=nodebld();
		}
	}
	else rc=nodebld();

	loginf("rc=%d",rc);

	return rc;
}

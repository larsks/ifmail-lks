#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#ifdef HAS_SYSLOG
#include <syslog.h>
#endif
#include "mygetopt.h"
#include "lutil.h"
#include "xutil.h"
#include "ftn.h"
#include "nodelist.h"
#include "trap.h"
#include "config.h"
#include "version.h"

#if defined(SHORT_PID_T)
#define pid_t short
#elif defined(INT_PID_T)
#define pid_t int
#endif

int master=0;
int forcedcalls=0;
char *forcedphone=NULL;
char *forcedline=NULL;
#if defined(HAS_TCP) || defined(HAS_TERM)
char *inetaddr=NULL;
#endif
#ifdef NOISEDEBUG
int junklevel=0;
#endif

int call(faddr *);
fa_list *callall(void);
int answer(char *);
void mkdirs(char*);

void usage(void)
{
#ifdef NOISEDEBUG
#if defined(HAS_TCP) || defined(HAS_TERM)
	confusage("-r<role> -j<num> -a<inetaddr> <node> ...");
#else
	confusage("-r<role> -j<num> <node> ...");
#endif
	fprintf(stderr,"-j<num>		damage every <num> byte	[%d]\n",
								junklevel);
#else
#if defined(HAS_TCP) || defined(HAS_TERM)
	confusage("-r<role> -a<inetaddr> <node> ...");
#else
	confusage("-r<role> <node> ...");
#endif
#endif
	fprintf(stderr,"-r 0|1		1 - master, 0 - slave	[0]\n");
	fprintf(stderr,"-n<phone>	forced phone number\n");
	fprintf(stderr,"-l<ttydevice>	forced tty device\n");
#if defined(HAS_TCP) || defined(HAS_TERM)
	fprintf(stderr,"-a<inetaddr>	use TCP/IP instead of modem\n");
#endif
	fprintf(stderr,"  <node>	should be in domain form, e.g. f11.n22.z3\n");
	fprintf(stderr,"		(this implies master mode)\n");
	fprintf(stderr,"\n or: %s tsync|yoohoo|**EMSI_INQC816\n",myname);
	fprintf(stderr,"		(this implies slave mode)\n");
}

int main(argc,argv)
int argc;
char *argv[];
{
	int c,uid;
	fa_list *callist=NULL,**tmpl;
	faddr *tmp;
	int rc,maxrc,callno=0,succno=0;
	char *answermode=NULL,*p;
#ifdef NEED_FORK
	pid_t child,waitret;
	int status;
#endif

#if defined(HAS_SYSLOG) && defined(CICOLOG)
	logfacility=CICOLOG;
#endif

	setmyname(argv[0]);
	catch(myname);
#ifdef NOISEDEBUG
#if defined(HAS_TCP) || defined(HAS_TERM)
	while ((c=getopt(argc,argv,"j:x:r:n:l:a:I:h")) != -1)
#else
	while ((c=getopt(argc,argv,"j:x:r:n:l:I:h")) != -1)
#endif
#else
#if defined(HAS_TCP) || defined(HAS_TERM)
	while ((c=getopt(argc,argv,"x:r:n:l:a:I:h")) != -1)
#else
	while ((c=getopt(argc,argv,"x:r:n:l:I:h")) != -1)
#endif
#endif
	if (confopt(c,optarg)) switch (c)
	{
#ifdef NOISEDEBUG
	case 'j':	junklevel=atoi(optarg); break;
#endif
	case 'r':	master=atoi(optarg);
			if ((master != 0) && (master != 1))
			{
				usage();
				exit(1);
			}
			break;
	case 'l':	forcedline=optarg; break;
#if defined(HAS_TCP) || defined(HAS_TERM)
	case 'a':	inetaddr=optarg; break;
#endif
	case 'n':	forcedphone=optarg; break;
	default:	usage(); exit(1);
	}

	if (readconfig())
	{
		fprintf(stderr,"Error getting configuration, aborting\n");
		exit(1);
	}

	maxpsize=0L; /* we want classic packet names here */

	tmpl=&callist;

	while (argv[optind])
	{
		for (p=argv[optind];(*p) && (*p == '*');p++);
		if ((strcasecmp(argv[optind],"tsync") == 0) ||
		    (strcasecmp(argv[optind],"yoohoo") == 0) ||
		    (strncasecmp(p,"EMSI_",5) == 0))
		{
			master=0;
			answermode=argv[optind];
			debug(10,"inbound \"%s\" mode",S(answermode));
		}
		else
		{
			debug(8,"callist entry \"%s\"",argv[optind]);
			if ((tmp=parsefaddr(argv[optind])))
			{
				*tmpl=(fa_list *)xmalloc(sizeof(fa_list));
				(*tmpl)->next=NULL;
				(*tmpl)->addr=tmp;
				tmpl=&((*tmpl)->next);
			}
			else logerr("unrecognizable address \"%s\"",argv[optind]);
		}
		optind++;
	}

	if (callist)
	{
		master=1;
		forcedcalls=1;
	}

	/*
	   The following witchkraft about uid-s is necessary to make
	   access() work right.  Unforunately, access() checks the real
	   uid, if ifcico is invoked with supervisor real uid (as when
	   called by uugetty) it returns X_OK for the magic files that
	   even do not have `x' bit set.  Therefore, `reference' magic
	   file requests are taken for `execute' requests (and the
	   actual execution natually fails).  Here we set real uid equal
	   to effective.  If real uid is not zero, all these fails, but
	   in this case it is not necessary anyway.
	*/

	uid=geteuid();
	seteuid(0);
	setuid(uid);
	seteuid(uid);
	debug(2,"uid=%d, euid=%d",getuid(),geteuid());

	umask(066); /* packets may contain confidential information */

#ifdef HAS_BSD_SIGNALS
	siginterrupt(SIGALRM,1);
	siginterrupt(SIGPIPE,1);
#endif

	p=xstrcpy(inbound);
	p=xstrcat(p,"/tmp/");
	mkdirs(p);
	free(p);

	maxrc=0;
	if (master)
	{

#ifdef NEED_FORK
		if ((child=fork()))
		{
			if (child == -1)
			{
				logerr("$fork() error");
				exit(1);
			}
			while (((waitret=wait(&status)) != -1) &&
			       (waitret != child))
			{
				logerr("wait return %d, status %d,%d",
					waitret,status>>8,status&0xff);
			}
			if (status&0xff) kill(getpid(),status&0xff);
			else exit(status>>8);
		}
#endif

		if (callist == NULL) callist=callall();
		for (tmpl=&callist;*tmpl;tmpl=&((*tmpl)->next))
		{
			callno++;
			rc=call((*tmpl)->addr);
			loginf("call to %s %s (rc=%d)",
				ascfnode((*tmpl)->addr,0x1f),
				rc?"failed":"successful",rc);
			if (rc > maxrc) maxrc=rc;
			if (rc == 0) succno++;
		}
	}
	else /* slave */
	{
		maxrc=answer(answermode);
		callno=1;
		succno=(maxrc == 0);
	}

	if (callno)
		loginf("%d of %d calls, maxrc=%d",succno,callno,maxrc);

	return maxrc;
}

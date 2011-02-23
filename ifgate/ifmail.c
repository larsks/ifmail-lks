#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sysexits.h>
#ifdef HAS_SYSLOG
#include <syslog.h>
#endif
#include "mygetopt.h"
#include "lutil.h"
#include "xutil.h"
#include "ftn.h"
#include "rfcaddr.h"
#include "falists.h"
#include "rfcmsg.h"
#include "ftnmsg.h"
#include "areas.h"
#include "config.h"
#include "version.h"
#include "trap.h"
#include "hash.h"

extern int newsmode;
extern char *configname;
extern char passwd[];

extern int putmessage(rfcmsg *,ftnmsg *,FILE *,faddr*,char,fa_list **);
extern void closepkt(void);
extern char *bgets(char *,int,FILE *);
extern void readareas(char *);
extern void try_attach(char *,int,faddr *,char);
extern int flag_on(char *,char *);
extern unsigned INT16 crc(char *);
extern void close_alias_db(void);
#ifdef HAS_NDBM_H
extern void close_id_db(void);
#endif

int usetmp=0;
faddr *bestaka;
extern int fakeoutbound;

void usage(void)
{
	confusage("-N -o<flavors> -n -r<addr> -g<grade> <recip> ...");
	fprintf(stderr,"-N		put packets to /tmp/ifmail directory\n");
	fprintf(stderr,"-o<flavors>	force `out' mode for these flavors\n");
	fprintf(stderr,"-o+		force `out' mode for all flavors\n");
	fprintf(stderr,"-o-		reset `out' mode for all flavors\n");
	fprintf(stderr,"-n		set news mode\n");
	fprintf(stderr,"-r<addr>	address to route packet\n");
	fprintf(stderr,"-g<grade>	[ n | c | h ] \"flavor\" of packet\n");
	fprintf(stderr,"<recip>		list of receipient addresses\n");
}

int main(argc,argv)
int argc;
char *argv[];
{
	int c;
	char *p;
	char buf[BUFSIZ];
	FILE *fp;
	char *routec=NULL;
	faddr *route = NULL;
	fa_list **envrecip, *envrecip_start = NULL;
	int envrecip_count=0;
	area_list *area = NULL, *area_start = NULL;
	int area_count=0;
	int msg_in=0,msg_out=0;
	rfcmsg *msg=NULL,*tmsg;
	ftnmsg *fmsg=NULL;
	faddr *taddr;
	char cflavor='\0',flavor;
	fa_list *sbl = NULL;
	unsigned long svmsgid;
	char *outmode=NULL;

#if defined(HAS_SYSLOG) && defined(MAILLOG)
	logfacility=MAILLOG;
#endif

	setmyname(argv[0]);
	catch(myname);
	while ((c=getopt(argc,argv,"g:r:x:I:nNho")) != -1)
	if (confopt(c,optarg)) switch (c)
	{
		case 'N':	fakeoutbound=1; break;
		case 'o':	outmode=optarg; break;
		case 'g':	if (optarg && ((*optarg == 'n') || 
				   (*optarg == 'c') || (*optarg == 'h')))
				{
					cflavor=*optarg;
				}
				else 
				{
					usage(); 
					exit(EX_USAGE);
				}
				break;
		case 'r':	routec=optarg; break;
		case 'n':	newsmode=1; break;
		default:	usage(); exit(EX_USAGE);
	}

	if (cflavor == 'n') cflavor='o';

	if (readconfig())
	{
		fprintf(stderr,"Error getting configuration, aborting\n");
		exit(EX_DATAERR);
	}

	if (outmode)
	{
		if (*outmode == '-')
			nonpacked="";
		else if (*outmode == '+')
			nonpacked="och";
		else
			nonpacked=outmode;
	}
	if (nonpacked == NULL) nonpacked="";
	for (p=nonpacked;*p;p++)
	if ((*p == 'f') || (*p == 'n')) *p='o';
	/* constant values will not be modified anyway */

	if ((routec) && ((route=parsefaddr(routec)) == NULL))
		logerr("unparsable route address \"%s\" (%s)",
			S(routec),addrerrstr(addrerror));

	if ((p=strrchr(argv[0],'/'))) p++;
	else p=argv[0];
	if (!strcmp(p,"ifnews")) newsmode=1;

	if (newsmode)
	{
		readareas(areafile);
#if defined(HAS_SYSLOG) && defined(NEWSLOG)
		logfacility=NEWSLOG;
#endif
	}
	else
	{
		if (strchr(nonpacked,'m'))
			nonpacked="och";
	}

	envrecip=&envrecip_start;
	while (argv[optind])
	if ((taddr=parsefaddr(argv[optind++]))) 
	{
		(*envrecip)=(fa_list*)xmalloc(sizeof(fa_list));
		(*envrecip)->next=NULL;
		(*envrecip)->addr=taddr;
		envrecip=&((*envrecip)->next);
		envrecip_count++;
	}
	else logerr("unparsable recipient \"%s\" (%s), ignored",
		argv[optind-1],
		addrerrstr(addrerror));

	if ((!newsmode) && (!envrecip_count))
	{
		logerr("No valid receipients specified, aborting");
		exit(EX_NOUSER);
	}

	if (!route && newsmode && envrecip_count)
		route=envrecip_start->addr;

	if (!route)
		if ((routec=getenv("NEWSSITE")))
			route=parsefaddr(routec);

	if (!route)
	{
		logerr("Routing address not specified, aborting");
		exit(EX_USAGE);
	}

	if (fakeoutbound)
	{
		mkdir("/tmp/ifmail",0777);
		loginf("packets will go to /tmp/ifmail");
	}

	bestaka=bestaka_s(route);

	for(envrecip=&envrecip_start;*envrecip;envrecip=&((*envrecip)->next))
		loginf("envrecip: %s",ascfnode((*envrecip)->addr,0x7f));
	loginf("route: %s",ascfnode(route,0x1f));

	umask(066); /* packets may contain confidential information */

	while (!feof(stdin))
	{
		usetmp=0;
		tidyrfc(msg);
		msg=parsrfc(stdin);

		flavor=cflavor;

		if (newsmode)
		{
			if (!flavor) flavor='o';
			tidy_arealist(area_start);
			area_start=areas(hdr("Newsgroups",msg));
			area_count=0;
			for(area=area_start;area;area=area->next)
			{
				area_count++;
				debug(9,"area: %s",S(area->name));
			}
			tidy_falist(&sbl);
			for (tmsg=msg;tmsg;tmsg=tmsg->next)
				if (strcasecmp(tmsg->key,"X-FTN-SEEN-BY") == 0)
					fill_list(&sbl,tmsg->val,NULL);
		}
		else
		{
			if ((p=hdr("X-FTN-FLAGS",msg)))
			{
				if (!flavor)
				{
					if (flag_on("CRS",p)) flavor='c';
					else if (flag_on("HLD",p)) flavor='h';
					else flavor='o';
				}
				if (flag_on("ATT",p))
					try_attach(hdr("Subject",msg),
						0,route,flavor);
				if (flag_on("TFS",p))
					try_attach(hdr("Subject",msg),
						1,route,flavor);
				if (flag_on("KFS",p))
					try_attach(hdr("Subject",msg),
						2,route,flavor);
			}
			if ((!flavor) && ((p=hdr("Priority",msg)) ||
			         (p=hdr("Precedence",msg)) ||
			         (p=hdr("X-Class",msg))))
			{
				while (isspace(*p)) p++;
				if ((strncasecmp(p,"fast",4) == 0) ||
				    (strncasecmp(p,"high",4) == 0) ||
				    (strncasecmp(p,"crash",5) == 0) ||
				    (strncasecmp(p,"airmail",5) == 0) ||
				    (strncasecmp(p,"special-delivery",5) == 0) ||
				    (strncasecmp(p,"first-class",5) == 0))
					flavor='c';
				else if ((strncasecmp(p,"slow",4) == 0) ||
				         (strncasecmp(p,"low",3) == 0) ||
				         (strncasecmp(p,"hold",4) == 0) ||
				         (strncasecmp(p,"news",4) == 0) ||
				         (strncasecmp(p,"bulk",4) == 0) ||
				         (strncasecmp(p,"junk",4) == 0))
					flavor='h';
			}
			if (!flavor) flavor='o';
		}

		if (((!newsmode) && (envrecip_count > 1)) ||
		    ((newsmode) && (area_count > 1)))
		{
			if ((fp=tmpfile()) == NULL)
			{
				logerr("$Cannot open temporary file");
				exit(EX_OSERR);
			}
			while(bgets(buf,sizeof(buf)-1,stdin))
				fputs(buf,fp);
			rewind(fp);
			usetmp=1;
		}
		else
		{
			fp=stdin;
			usetmp=0;
		}

		if (newsmode && ((area_count < 1) ||
		    (maxgroups && (group_count > maxgroups)) ||
		    hdr("Control",msg) ||
		     (in_list(route,&sbl))))
		{
			debug(9,"skipping news message");
			while(bgets(buf,sizeof(buf)-1,fp));
		}
		else
		{
			tidy_ftnmsg(fmsg);
			if ((fmsg=mkftnhdr(msg)) == NULL)
			{
				logerr("Unable to create FTN headers from RFC ones, aborting");
				exit(EX_UNAVAILABLE);
			}

			if (newsmode)
			{
				fill_list(&sbl,ascfnode(bestaka,0x06),NULL);
				fill_list(&sbl,ascfnode(route,0x06),NULL);
				fmsg->to->zone=route->zone;
				fmsg->to->net=route->net;
				fmsg->to->node=route->node;
				fmsg->to->point=route->point;
			}

			if (!newsmode)
			for(envrecip=&envrecip_start;*envrecip;envrecip=&((*envrecip)->next))
			{
				fmsg->to=(*envrecip)->addr;
				if (putmessage(msg,fmsg,fp,route,flavor,&sbl))
				{
					logerr("Unable to put netmail message into the packet, aborting");
					exit(EX_CANTCREAT);
				}
				if (usetmp) rewind(fp);
				fmsg->to=NULL;
				msg_out++;
			}
			else
			for(area=area_start;area;area=area->next)
			{
				fmsg->area=area->name;
				svmsgid=fmsg->msgid_n;
				hash_update_s(&fmsg->msgid_n,
						area->name);
				if (putmessage(msg,fmsg,fp,route,flavor,&sbl))
				{
					logerr("Unable to put echo message into the packet, aborting");
					exit(EX_SOFTWARE);
				}
				if (usetmp) rewind(fp);
				fmsg->area=NULL;
				fmsg->msgid_n=svmsgid;
				msg_out++;
			}
			msg_in++;
		}
		if (usetmp) fclose(fp);
	}

	closepkt();
	close_alias_db();
#ifdef HAS_NDBM_H
	close_id_db();
#endif

	loginf("end input %d, output %d messages",msg_in,msg_out);

	return EX_OK;
}

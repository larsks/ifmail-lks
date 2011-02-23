#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#ifdef HAS_SYSLOG
#include <syslog.h>
#endif
#include "mygetopt.h"
#include "lutil.h"
#include "xutil.h"
#include "ftn.h"
#include "config.h"
#include "scanout.h"
#include "version.h"
#include "trap.h"

void usage(name)
char *name;
{
	confusage("");
}

static struct _alist {
	struct _alist *next;
	faddr addr;
	int flavors;
	time_t time;
	off_t size;
} *alist=NULL;

#define F_NORMAL 1
#define F_CRASH  2
#define F_HOLD   4
#define F_FREQ   8

static int each(faddr*,char,int,char*);

int main(argc,argv)
int argc;
char *argv[];
{
	int c,rc;
	struct _alist *tmp;
	char flstr[5];
	time_t age;
	char *unit;

#if defined(HAS_SYSLOG) && defined(MAILLOG)
	logfacility=MAILLOG;
#endif

	setmyname(argv[0]);
	catch(myname);
	while ((c=getopt(argc,argv,"x:l:h")) != -1)
	if (confopt(c,optarg)) switch (c)
	{
		default:	usage(); exit(1);
	}

	if ((rc=readconfig()))
	{
		fprintf(stderr,"Error getting configuration, aborting\n");
		return rc;
	}

	if ((rc=scanout(each)))
	{
		fprintf(stderr,"Error scanning outbound, aborting\n");
		return rc;
	}

	printf("flavor   size   age\t\taddress\n");
	for (tmp=alist;tmp;tmp=tmp->next)
	if ((tmp->flavors & F_FREQ) || (tmp->size))
	{
		strcpy(flstr,"....");
		if ((tmp->flavors) & F_CRASH ) flstr[0]='C';
		if ((tmp->flavors) & F_NORMAL) flstr[1]='N';
		if ((tmp->flavors) & F_HOLD  ) flstr[2]='H';
		if ((tmp->flavors) & F_FREQ  ) flstr[3]='R';

		(void)time(&age);
		age-=tmp->time;
		if (age > (30L*24L*60L*60L))
			{ age /= (30L*24L*60L*60L); unit="month"; }
		else if (age > (7L*24L*60L*60L))
			{ age /= (7L*24L*60L*60L); unit="week"; }
		else if (age > (24L*60L*60L))
			{ age /= (24L*60L*60L); unit="day"; }
		else if (age > (60L*60L))
			{ age /= (60L*60L); unit="hour"; }
		else if (age > 60L)
			{ age /= 60L; unit="minute"; }
		else 
			{ unit="second"; }

		printf("%s %8lu %3d %s%s   \t%s\n",
			flstr,(long)tmp->size,(int)age,unit,(age==1)?" ":"s",
			ascfnode(&(tmp->addr),0x1f));
	}

	return 0;
}

static int each(addr,flavor,isflo,fname)
faddr *addr;
char flavor;
int isflo;
char *fname;
{
	struct _alist **tmp;
	struct stat st;
	FILE *fp;
	char buf[256],buf2[256],*p,*q;

	if ((isflo != OUT_PKT) && (isflo != OUT_FLO) && (isflo != OUT_REQ))
		return 0;

	for (tmp=&alist;*tmp;tmp=&((*tmp)->next))
		if (((*tmp)->addr.zone == addr->zone) &&
		    ((*tmp)->addr.net == addr->net) &&
		    ((*tmp)->addr.node == addr->node) &&
		    ((*tmp)->addr.point == addr->point) &&
		    (((*tmp)->addr.domain == NULL) ||
		     (addr->domain == NULL) ||
		     (strcasecmp((*tmp)->addr.domain,addr->domain) == 0)))
			break;
	if (*tmp == NULL)
	{
		*tmp=(struct _alist *)xmalloc(sizeof(struct _alist));
		(*tmp)->next=NULL;
		(*tmp)->addr.name=NULL;
		(*tmp)->addr.zone=addr->zone;
		(*tmp)->addr.net=addr->net;
		(*tmp)->addr.node=addr->node;
		(*tmp)->addr.point=addr->point;
		(*tmp)->addr.domain=addr->domain;
		(*tmp)->flavors=0;
		time(&((*tmp)->time));
		(*tmp)->size=0L;
	}
	if ((isflo == OUT_FLO) || (isflo == OUT_PKT)) switch (flavor)
	{
	case '?':	break;
	case 'o':	(*tmp)->flavors |= F_NORMAL; break;
	case 'c':	(*tmp)->flavors |= F_CRASH; break;
	case 'h':	(*tmp)->flavors |= F_HOLD; break;
	default:	fprintf(stderr,"Unknown flavor: '%c'\n",flavor); break;
	}
	if (stat(fname,&st) != 0)
	{
		perror("cannot stat");
		st.st_size=0L;
		(void)time(&st.st_mtime);
	}
	if (st.st_mtime < (*tmp)->time) (*tmp)->time = st.st_mtime;
	if (isflo == OUT_FLO)
	{
		if ((fp=fopen(fname,"r")))
		{
			while (fgets(buf,sizeof(buf)-1,fp))
			{
				if (*(p=buf+strlen(buf)-1) == '\n') *p--='\0';
				while (isspace(*p)) *p--='\0';
				for (p=buf;*p;p++) if (*p == '\\') *p='/';
				for (p=buf;*p && isspace(*p);p++);
				if (*p == '~') continue;
				if ((*p == '#') ||
				    (*p == '-') ||
				    (*p == '^') ||
				    (*p == '@')) p++;

				if (dosoutbound &&
				    (strncasecmp(p,dosoutbound,
						strlen(dosoutbound)) == 0)) {
					strcpy(buf2,outbound);
					for (p+=strlen(dosoutbound),
						q=buf2+strlen(buf2);
					     *p;p++,q++)
						*q=((*p) == '\\')?'/'
								:tolower(*p);
					*q='\0';
					p=buf2;
				}

				if (stat(p,&st) != 0)
				{
					perror("cannot stat");
					st.st_size=0L;
					(void)time(&st.st_mtime);
				}
				if ((p=strrchr(fname,'/'))) p++;
				else p=fname;
				if ((strlen(p) == 12) &&
				    (strspn(p,"0123456789abcdefABCDEF") == 8) &&
				    (p[8] == '.'))
				{
					if (st.st_mtime < (*tmp)->time) 
						(*tmp)->time = st.st_mtime;
				}
				(*tmp)->size += st.st_size;
			}
			fclose(fp);
		}
		else perror("cannot open flo");
	}
	else if (isflo == OUT_PKT)
	{
		(*tmp)->size += st.st_size;
	}
	else if (isflo == OUT_REQ)
	{
		(*tmp)->flavors |= F_FREQ;
	}
	return 0;
}

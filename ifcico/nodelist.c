#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#ifdef HAS_NDBM_H
#include <fcntl.h>
#endif
#include "directory.h"

#include "xutil.h"
#include "lutil.h"
#include "ftn.h"
#include "config.h"
#include "nodelist.h"
#include "nlindex.h"

#define NULLDOMAIN "nulldomain"

node *getnlent(addr)
faddr *addr;
{
	FILE *fp;
	static node nodebuf;
	static char buf[256],*p,*q;
	struct _ixentry xaddr;
	struct _loc *loc;
	off_t off;
	datum key;
	datum dat;
	int n,i,j,found=-1,maybe=-1;
	int ixflag,stdflag;
	char *dom,*mydomain;

	debug(20,"getnlent for %s",ascfnode(addr,0x1f));

	mydomain=whoami->addr->domain;
	if (mydomain == NULL) mydomain=NULLDOMAIN;

	key.dptr=(char*)&xaddr;
	key.dsize=sizeof(struct _ixentry);

	nodebuf.addr.domain=NULL;
	nodebuf.addr.zone=0;
	nodebuf.addr.net=0;
	nodebuf.addr.node=0;
	nodebuf.addr.point=0;
	nodebuf.addr.name=NULL;
	nodebuf.hub=0;
	nodebuf.type=0;
	nodebuf.pflag=0;
	nodebuf.name=NULL;
	nodebuf.location=NULL;
	nodebuf.sysop=NULL;
	nodebuf.phone=NULL;
	nodebuf.speed=0;
	nodebuf.flags=0L;
	nodebuf.uflags[0]=NULL;

	if (addr == NULL) goto retdummy;

	if (addr->zone == 0)
		addr->zone=whoami->addr->zone;
	xaddr.zone=addr->zone;
	nodebuf.addr.zone=addr->zone;
	xaddr.net=addr->net;
	nodebuf.addr.net=addr->net;
	xaddr.node=addr->node;
	nodebuf.addr.node=addr->node;
	xaddr.point=addr->point;
	nodebuf.addr.point=addr->point;

	switch (initnl())
	{
	case 0:	break;
	case 1:	loginf("WARNING: nodelist index needs to be rebuilt with \"ifindex\"");
		break;
	default: goto retdummy;
	}

#ifdef HAS_NDBM_H
	dat=dbm_fetch(nldb,key);
#else
	dat=fetch(key);
#endif
	if (dat.dptr == NULL) goto retdummy;
	n=dat.dsize/sizeof(struct _loc);
	debug(20,"found %d entries",n);
	loc=(struct _loc *)dat.dptr;
	for (i=0;(i<n) && (found == -1);i++)
	{
		dom=nodevector[loc[i].nlnum].domain;
		if (dom == NULL) dom=NULLDOMAIN;
		off=loc[i].off;
		if ((addr->domain != NULL) &&
		    (strcasecmp(addr->domain,dom) == 0))
		{
			debug(20,"strict match for domain \"%s\"",dom);
			found=i;
		}
		else if ((addr->domain == NULL) &&
		    (strcasecmp(mydomain,dom) == 0))
		{
			debug(20,"match with home domain \"%s\"",dom);
			found=i;
			break;
		}
		else if (addr->domain == NULL)	
		{
			debug(20,"probable match with domain \"%s\"",dom);
			maybe=i;
			break;
		}
	}
	if (found == -1) found=maybe;
	if (found == -1) 
	{
		debug(20,"Match not found");
		goto retdummy;
	}
	fp=nodevector[loc[found].nlnum].fp;
	if (fseek(fp,loc[found].off,SEEK_SET) != 0)
	{
		logerr("$seek failed for nodelist entry %d",loc[found].nlnum);
	}
	if (fgets(buf,sizeof(buf)-1,fp) == NULL)
	{
		logerr("$fgets failed for nodelist entry %d",loc[found].nlnum);
	}
	if (*(p=buf+strlen(buf)-1) == '\n') *p='\0';
	if (*(p=buf+strlen(buf)-1) == '\r') *p='\0';
	for (p=buf;*p;p++) if (*p == '_') *p=' ';
	debug(20,"Nodelist line: \"%s\"",buf);
	p=buf;

	if ((q=strchr(p,','))) *q++='\0';
	nodebuf.type=NL_NONE;
	if (p[0] == '\0') nodebuf.type=NL_NODE;
	else for (j=0;pkey[j].key;j++)
		if (strcasecmp(p,pkey[j].key) == 0)
		{
			nodebuf.type=pkey[j].type;
			nodebuf.pflag=pkey[j].pflag;
			break;
		}
	if (nodebuf.type == NL_NONE)
	{
		for (q=buf;*q;q++) if (*q < ' ') *q='.';
		logerr("nodelist %d offset +%lu: unidentified entry \"%s\"",
			loc[found].nlnum,(unsigned long)loc[found].off,buf);
		goto retdummy;
	}
	p=q;
	if (p == NULL) goto badsyntax;
	if ((q=strchr(p,','))) *q++='\0';
	p=q;
	if (p == NULL) goto badsyntax;
	if ((q=strchr(p,','))) *q++='\0';
	nodebuf.name=p;
	p=q;
	if (p == NULL) goto badsyntax;
	if ((q=strchr(p,','))) *q++='\0';
	nodebuf.location=p;
	p=q;
	if (p == NULL) goto badsyntax;
	if ((q=strchr(p,','))) *q++='\0';
	nodebuf.sysop=p;
	p=q;
	if (p == NULL) goto badsyntax;
	if ((q=strchr(p,','))) *q++='\0';
	if (strcasecmp(p,"-Unpublished-") == 0)
		nodebuf.phone=NULL;
	else
		nodebuf.phone=p;
	p=q;
	if (p == NULL) goto badsyntax;
	if ((q=strchr(p,','))) *q++='\0';
	nodebuf.speed=atoi(p);
	ixflag=0;
	for (p=q;p;p=q)
	{
		if ((q=strchr(p,','))) *q++='\0';
		stdflag=0;
		for (j=0;fkey[j].key;j++)
			if (strcasecmp(p,fkey[j].key) == 0)
			{
				stdflag=1;
				nodebuf.flags|=fkey[j].flag;
			}
		if (!stdflag && (ixflag < MAXUFLAGS))
		{
			nodebuf.uflags[ixflag++]=p;
			if (ixflag < MAXUFLAGS) nodebuf.uflags[ixflag]=NULL;
		}
	}
	nodebuf.addr.name=nodebuf.sysop;
	nodebuf.addr.domain=nodevector[loc[found].nlnum].domain;
	nodebuf.hub=loc[found].hub;
	if (addr->domain == NULL) addr->domain=xstrcpy(nodebuf.addr.domain);

	debug(20,"getnlent: type		%d, pflag=%x",nodebuf.type,nodebuf.pflag);
	debug(20,"getnlent: name		%s",nodebuf.name);
	debug(20,"getnlent: location	%s",nodebuf.location);
	debug(20,"getnlent: sysop	%s",nodebuf.sysop);
	debug(20,"getnlent: phone	%s",nodebuf.phone);
	debug(20,"getnlent: speed	%u",nodebuf.speed);
	debug(20,"getnlent: flags	0x%lx",nodebuf.flags);
	for (j=0; (j < MAXUFLAGS) && nodebuf.uflags[j];j++)
		debug(20,"getnlent: uflag	%s",nodebuf.uflags[j]);

	return &nodebuf;

badsyntax:
	logerr("nodelist %d offset +%lu: bad syntax in line \"%s\"",
		loc[found].nlnum,(unsigned long)loc[found].off,buf);
	/* fallthrough */
retdummy:
	debug(20,"getnlent returns dummy entry");
	nodebuf.type=0;
	nodebuf.pflag=NL_DUMMY;
	nodebuf.name="Unknown";
	nodebuf.location="Nowhere";
	nodebuf.sysop="Sysop";
	nodebuf.phone=NULL;
	nodebuf.speed=2400;
	nodebuf.flags=0L;
	nodebuf.uflags[0]=NULL;
	
	return &nodebuf;
}

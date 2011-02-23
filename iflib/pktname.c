/* This module hopefully implements BinkleyTerm packet naming conventions */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include "directory.h"
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <time.h>
#include "lutil.h"
#include "xutil.h"
#include "ftn.h"
#include "config.h"

#ifndef PATH_MAX
#define PATH_MAX 512
#endif

#define FAKEOUT "/tmp/ifmail/"
#define ptyp "ut"
#define ftyp "lo"
#define ttyp "pk"
#define rtyp "req"
#define styp "spl"
#define btyp "bsy"
#define qtyp "sts"

extern unsigned INT32 sequencer(void);

int fakeoutbound=0;
static char buf[PATH_MAX];

static char *prepbuf(faddr*);
static char *prepbuf(addr)
faddr *addr;
{
	char *p,*domain=NULL;
	char zpref[8];
	fa_list *tmpl;

	if (fakeoutbound)
	{
		strcpy(buf,FAKEOUT);
		if (whoami->addr->domain)
			strcat(buf,whoami->addr->domain);
		else
			strcat(buf,"fidonet");
	}
	else
	{
		strcpy(buf,outbound);
	}

	if (addr->domain) domain=addr->domain;
	else for (tmpl=nllist;tmpl;tmpl=tmpl->next)
		if (tmpl->addr->zone == addr->zone)
		{
			domain=tmpl->addr->domain;
			break;
		}
	debug(1,"using domain \"%s\" for outbound",S(domain));
	if ((domain != NULL) && (whoami->addr->domain != NULL) &&
	    (strcasecmp(domain,whoami->addr->domain) != 0))
	{
		debug(1,"this is not our primary domain");
		if ((p=strrchr(buf,'/'))) p++;
		else p=buf;
		strcpy(p,domain);
		for (;*p;p++) *p=tolower(*p);
		for (tmpl=nllist;tmpl;tmpl=tmpl->next)
			if ((tmpl->addr->domain) &&
			    (strcasecmp(tmpl->addr->domain,domain) == 0))
				break;
		if (tmpl && (tmpl->addr->zone == addr->zone))
			zpref[0]='\0';
		else sprintf(zpref,".%03x",addr->zone);
	}
	else /* this is our primary domain */
	{
		debug(1,"this is our primary domain");
		if ((addr->zone == 0) || (addr->zone == whoami->addr->zone))
			zpref[0]='\0';
		else sprintf(zpref,".%03x",addr->zone);
	}

	p=buf+strlen(buf);

	if (addr->point)
		sprintf(p,"%s/%04x%04x.pnt/%08x.",
			zpref,addr->net,addr->node,addr->point);
	else
		sprintf(p,"%s/%04x%04x.",zpref,addr->net,addr->node);

	p=buf+strlen(buf);
	return p;
}

char *pktname(addr,flavor)
faddr *addr;
char flavor;
{
	char *p;
	struct stat st;
	char newname[PATH_MAX];

	if (is_local(addr) && !fakeoutbound)
	{
		debug(1,"local address, put packet into inbound");
		strncpy(buf,protinbound,sizeof(buf)-10);
		sprintf(buf+strlen(buf),"/%08lx.pkt",
				(unsigned long)sequencer());
	}
	else
	{
		p=prepbuf(addr);
		if (flavor == 'f') flavor='o';
		if (maxpsize && (strchr(nonpacked,flavor) == 0))
		{
			sprintf(p,"%c%s/current.tmp",flavor,ttyp);
			if ((stat(buf,&st) == 0) &&
			    (st.st_size >= maxpsize))
			{
				strcpy(newname,buf);
				sprintf(newname+strlen(newname)-11,
					"%08lx.pkt",(unsigned long)sequencer());
				if (rename(buf,newname))
					logerr("$error renaming \"%s\" to \"%s\"",
						S(buf),S(newname));
			}
		}
		else
		{
			sprintf(p,"%c%s",flavor,ptyp);
		}
	}
	debug(1,"packet name is \"%s\"",buf);
	return buf;
}

char *floname(addr,flavor)
faddr *addr;
char flavor;
{
	char *p;

	p=prepbuf(addr);
	if (flavor == 'o') flavor='f';
	sprintf(p,"%c%s",flavor,ftyp);
	debug(1,"flo file name is \"%s\"",buf);
	return buf;
}

char *reqname(addr,flavor)
faddr *addr;
char flavor;
{
	char *p;

	p=prepbuf(addr);
	if (flavor == 'o') flavor='f'; /* in fact this is ignored for reqs */
	sprintf(p,"%s",rtyp);
	debug(1,"req file name is \"%s\"",buf);
	return buf;
}

char *splname(addr,flavor)
faddr *addr;
char flavor;
{
	char *p;

	p=prepbuf(addr);
	if (flavor == 'o') flavor='f'; /* in fact this is ignored for spls */
	sprintf(p,"%s",styp);
	debug(1,"spl file name is \"%s\"",buf);
	return buf;
}

char *bsyname(addr,flavor)
faddr *addr;
char flavor;
{
	char *p;

	p=prepbuf(addr);
	if (flavor == 'o') flavor='f'; /* in fact this is ignored for bsys */
	sprintf(p,"%s",btyp);
	debug(1,"bsy file name is \"%s\"",buf);
	return buf;
}

char *stsname(addr,flavor)
faddr *addr;
char flavor;
{
	char *p;

	p=prepbuf(addr);
	if (flavor == 'o') flavor='f'; /* in fact this is ignored for bsys */
	sprintf(p,"%s",qtyp);
	debug(1,"sts file name is \"%s\"",buf);
	return buf;
}

char *pkdname(addr,flavor)
faddr *addr;
char flavor;
{
	char *p;

	p=prepbuf(addr);
	if (flavor == 'f') flavor='o';
	sprintf(p,"%c%s",flavor,ttyp);
	while ((p > buf) && (*p != '/')) p--;
	p++;
	debug(1,"pkd name is \"%s\"",p);
	return p;
}

static char *dow[] = {"su","mo","tu","we","th","fr","sa"};

char *arcname(addr,flavor)
faddr *addr;
char flavor;
{
	char *p;
	char *ext;
	time_t tt;
	struct tm *ptm;
	faddr *bestaka;

	(void)time(&tt);
	ptm=localtime(&tt);
	ext=dow[ptm->tm_wday];

	bestaka=bestaka_s(addr);

	(void)prepbuf(addr);
	p=strrchr(buf,'/');
	if (addr->point)
	{
		sprintf(p,"/%04x%04x.%s0",
			((bestaka->net) - (addr->net)) & 0xffff,
			((bestaka->node) - (addr->node) +
				(addr->point)) & 0xffff,
			ext);
	}
	else
	{
		sprintf(p,"/%04x%04x.%s0",
			((bestaka->net) - (addr->net)) & 0xffff,
			((bestaka->node) - (addr->node)) & 0xffff,
			ext);
	}
	debug(1,"arc file name is \"%s\" (flavor '%c' ignored)",buf,flavor);
	return buf;
}

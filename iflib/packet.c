#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include "xutil.h"
#include "lutil.h"
#include "ftn.h"
#include "config.h"
#include "bwrite.h"

#define HDR_RATE	19200
#define HDR_VER		2
#define HDR_PRODX	0x0000
#define HDR_FILLER	0x0000
#define HDR_CAPVALID	0x0100
#define HDR_PRODCODE	0x0201
#define HDR_CAPWORD	0x0001
#define HDR_PRODDATA	0L

char passwd[9] = "\0\0\0\0\0\0\0\0";
static FILE *pktfp=NULL;
static faddr pktroute = 
{
	NULL,0,0,0,0,NULL
};

void closepkt(void);

extern char *pktname(faddr *,char);
extern char *floname(faddr *,char);

#ifndef PATH_MAX
#define PATH_MAX 512
#endif

void mkdirs(name)
char *name;
{
	char buf[PATH_MAX],*p,*q;
	int rc;

	strncpy(buf,name,sizeof(buf)-1);
	buf[sizeof(buf)-1]='\0';
	debug(2,"creating directory tree for \"%s\"",buf);
	p=buf+1;
	while ((q=strchr(p,'/')))
	{
		*q='\0';
		rc=mkdir(buf,0700);
		debug(2,"mkdir(\"%s\") errno %d",buf,rc?errno:0);
		*q='/';
		p=q+1;
	}
}

FILE *openpkt(pkt,addr,flavor)
FILE *pkt;
faddr *addr;
char flavor;
{
	off_t pos;
	struct flock fl;
	struct stat st;
	char *name;
	struct tm *ptm;
	fa_list *pa;
	time_t t;
	int i;
	faddr *bestaka;

	debug(3,"openpkt entered");

	if (pkt == NULL)
	{
		if (pktfp)
		{
			debug(3,"packet opened, check address");
			if (metric(addr,&pktroute) == 0)
			{
				debug(3,"same address");
				if ((maxpsize == 0L) ||
				    ((fstat(fileno(pktfp),&st) == 0) &&
				     (st.st_size < maxpsize)))
				{
					debug(3,"return existing fp");
					return pktfp;
				}
				debug(3,"packet too big, open new");
				closepkt();
			}
			else
			{
				debug(3,"address changed, closing fp");
				closepkt();
			}
		}

		debug(3,"open new packet file");
		pktroute.zone=addr->zone;
		pktroute.net=addr->net;
		pktroute.node=addr->node;
		pktroute.point=addr->point;
		pktroute.domain=xstrcpy(addr->domain);
		pktroute.name=NULL;

		for (pa=pwlist;pa;pa=pa->next)
		if (metric(pa->addr,addr) == 0)
			strncpy(passwd,pa->addr->name,8);

		name=pktname(addr,flavor);

		mkdirs(name);

		if ((pktfp=fopen(name,"r+")) == NULL)
			pktfp=fopen(name,"w");
		if (pktfp == NULL)
		{
			logerr("$Unable to open packet %s",S(name));
			return NULL;
		}
		fl.l_type=F_WRLCK;
		fl.l_whence=0;
		fl.l_start=0L;
		fl.l_len=0L;
		if (fcntl(fileno(pktfp),F_SETLKW,&fl) < 0)
		{
			logerr("$Unable to lock packet %s",S(name));
			fclose(pktfp);
			return NULL;
		}
		pkt=pktfp;
		pos=fseek(pkt,-2L,SEEK_END);
		debug(2,"fseek (pkt,-2L,SEEK_END) result is %ld",(long)pos);
	}
	pos=ftell(pkt);
	debug(3,"ftell (pkt) result is %ld",(long)pos);
	if (pos <= 0L)
	{
#ifdef NEGATIVE_SEEK_BUG
		fseek(pkt,0L,SEEK_SET);
#endif
		time(&t);
		ptm=localtime(&t);
		bestaka=bestaka_s(addr);
		iwrite(bestaka->node,pkt);
		iwrite(addr->node        ,pkt);
		iwrite(ptm->tm_year+1900 ,pkt);
		iwrite(ptm->tm_mon       ,pkt);
		iwrite(ptm->tm_mday      ,pkt);
		iwrite(ptm->tm_hour      ,pkt);
		iwrite(ptm->tm_min       ,pkt);
		iwrite(ptm->tm_sec       ,pkt);
		iwrite(HDR_RATE          ,pkt);
		iwrite(HDR_VER           ,pkt);
		iwrite(bestaka->net ,pkt);
		iwrite(addr->net         ,pkt);
		iwrite(HDR_PRODX         ,pkt);
		for (i=0;i<8;i++) putc(passwd[i],pkt);
		iwrite(bestaka->zone,pkt);
		if (addr->zone) iwrite(addr->zone,pkt);
		else iwrite(bestaka->zone,pkt);
		iwrite(HDR_FILLER        ,pkt);
		iwrite(HDR_CAPVALID      ,pkt);
		iwrite(HDR_PRODCODE      ,pkt);
		iwrite(HDR_CAPWORD       ,pkt);
		iwrite(bestaka->zone,pkt);
		if (addr->zone) iwrite(addr->zone,pkt);
		else iwrite(bestaka->zone,pkt);
		iwrite(bestaka->point,pkt);
		iwrite(addr->point       ,pkt);
		lwrite(HDR_PRODDATA      ,pkt);
	}

	return pkt;
}

FILE *openflo(addr,flavor)
faddr *addr;
char flavor;
{
	FILE *flo;
	struct flock fl;
	char *name;

	debug(3,"openflo entered");

	name=floname(addr,flavor);

	mkdirs(name);

	if ((flo=fopen(name,"r+")) == NULL)
		flo=fopen(name,"w");
	if (flo == NULL)
	{
		logerr("$Unable to open flo file %s",S(name));
		return NULL;
	}
	fl.l_type=F_WRLCK;
	fl.l_whence=0;
	fl.l_start=0L;
	fl.l_len=0L;
	if (fcntl(fileno(flo),F_SETLKW,&fl) < 0)
	{
		logerr("$Unable to lock flo file %s",S(name));
		fclose(flo);
		return NULL;
	}

	return flo;
}

void closepkt(void)
{
	debug(3,"closepkt entered");

	if (pktfp)
	{
		iwrite(0,pktfp);
		fclose(pktfp);	/* close also discards lock */
	}
	pktfp=NULL;
	if (pktroute.domain) free(pktroute.domain);
}

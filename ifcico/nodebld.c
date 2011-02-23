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

int nodebld(void)
{
	int typ,impltyp=NL_NODE,num,i,j,rc=0;
	int lineno,entries,total;
	fa_list *tmp;
#ifndef HAS_BSD_DB
	FILE *ifp;
#endif
	off_t off;
	struct _loc *loc,*oldloc;
	unsigned short hub;
	unsigned char pflag;
	char buf[256],*p,*q;
	struct _ixentry addr;
	faddr *tmpa;
	datum key;
	datum dat;
	char *nm;

	loginf("Nodelist index rebuild requested");

	key.dptr=(char*)&addr;
	key.dsize=sizeof(struct _ixentry);

#ifndef DONT_HAVE_DBMCLOSE
#ifdef HAS_NDBM_H
	if (nldb != NULL)
	{
		dbm_close(nldb);
		nldb=NULL;
	}
#else
	dbmclose();
#endif
#endif

	nm=xstrcpy(nlbase);
	nm=xstrcat(nm,INDEX);

#ifdef HAS_BSD_DB
	nm=xstrcat(nm,".db");
	unlink(nm);
	nm[strlen(nm)-3]='\0';
#else
	nm=xstrcat(nm,".dir");
	if ((ifp=fopen(nm,"w"))) fclose(ifp);
	else
	{
		logerr("$cannot create new %s",S(nm));
		return 1;
	}
	strcpy(nm+strlen(nm)-3,"pag");
	if ((ifp=fopen(nm,"w"))) fclose(ifp);
	else
	{
		logerr("$cannot create new %s",S(nm));
		return 1;
	}
	nm[strlen(nm)-4]='\0';
#endif
#ifdef HAS_NDBM_H
	if ((nldb=dbm_open(nm,O_RDWR | O_CREAT,0600)) == NULL) rc=1;
#else
	rc=dbminit(nm);
#endif
	if (rc)
	{
		logerr("$cannot open dbm \"%s\"",S(nm));
		free(nm);
		return rc;
	}
	free(nm);

	rc=0;
	total=0;
	for (i=0,tmp=nllist;tmp;tmp=tmp->next,i++)
	if (nodevector[i].fp)
	{
		debug(20,"making index for \"%s[.???]\"",S(tmp->addr->name));
		addr.zone=0;
		addr.net=0;
		addr.node=0;
		addr.point=0;
		hub=0;
		if (tmp->addr)
		{
			addr.zone=tmp->addr->zone;
			addr.net=tmp->addr->net;
			addr.node=tmp->addr->node;
			addr.point=tmp->addr->point;
		}
		entries=0;
		lineno=0;
		while (!feof(nodevector[i].fp))
		{
			off=ftell(nodevector[i].fp);
			lineno++;
			if (fgets(buf,sizeof(buf)-1,nodevector[i].fp) == NULL)
				continue;
			if (*(buf+strlen(buf)-1) != '\n')
			{
				while (fgets(buf,sizeof(buf)-1,nodevector[i].fp) &&
				       (*(buf+strlen(buf)-1) != '\n')) /*void*/;
				logerr("nodelist %d(%u): too long line junked",
					i,lineno);
				continue;
			}
			if (*(p=buf+strlen(buf)-1) == '\n') *p--='\0';
			if (*p == '\r') *p='\0';
			if ((buf[0] == ';') || (buf[0] == '\032') ||
			    (buf[0] == '\0')) continue;
			if ((p=strchr(buf,','))) *p++='\0';
			if ((q=strchr(p,','))) *q++='\0';
			typ=NL_NONE;
			pflag=0;
			if (buf[0] == '\0') typ=impltyp;
			else if (strcasecmp(buf,"Boss") == 0) 
			{
				impltyp=NL_POINT;
				if ((tmpa=parsefnode(p)) == NULL)
				{
					logerr("%s(%u): unparsable Boss addr \"%s\"",
						S(tmp->addr->name),lineno,p);
					continue;
				}
				if (tmpa->zone) addr.zone=tmpa->zone;
				addr.net=tmpa->net;
				addr.node=tmpa->node;
				tidy_faddr(tmpa);
				typ=NL_NONE;
				continue; /* no further processing */
			}
			else
			{
				impltyp=NL_NODE;
				for (j=0;pkey[j].key;j++)
				if (strcasecmp(buf,pkey[j].key) == 0) 
				{
					typ=pkey[j].type;
					pflag=pkey[j].pflag;
				}
			}
			if (typ == NL_NONE)
			{
				for (q=buf;*q;q++) if (*q < ' ') *q='.';
				logerr("%s(%u): unidentified entry \"%s\"",
					S(tmp->addr->name),lineno,buf);
				continue;
			}
			debug(21,"got \"%s\" as \"%s\" typ %d",S(buf),S(p),typ);
			if ((num=atoi(p)) == 0)
			{
				logerr("%s(%u): bad numeric \"%s\"",
					S(tmp->addr->name),lineno,S(p));
				continue;
			}

			/* first check it for being a hub */

			if (typ == NL_NODE)
			{
				if (pflag == NL_HUB) hub=num;
				/* else it is under the same hub */
			}
			else hub=0; /* reset hub assignment */

			/* now update the current address */

			switch (typ)
			{
			case NL_ZONE:	addr.zone=num;
					addr.net=num;
					addr.node=0;
					addr.point=0;
					break;
			case NL_NET:	addr.net=num;
					addr.node=0;
					addr.point=0;
					break;
			case NL_NODE:	addr.node=num;
					addr.point=0;
					break;
			case NL_POINT:	addr.point=num;
					break;
			}
			debug(21,"put: %u:%u/%u.%u as (%u,%lu)",
				addr.zone,addr.net,addr.node,
				addr.point,i,(unsigned long)off);
#ifdef HAS_NDBM_H
			dat=dbm_fetch(nldb,key);
#else
			dat=fetch(key);
#endif
			oldloc=(struct _loc *)dat.dptr;
			/* FIXME: must check multiple entries in oldloc */
			if (oldloc &&
			    (tmp->addr->domain) &&
			    (nodevector[oldloc->nlnum].domain) &&
			    strcasecmp(nodevector[oldloc->nlnum].domain,
					tmp->addr->domain))
			{ /* same addr from another domain */
				loc=(struct _loc *)xmalloc(sizeof(struct _loc)+
					dat.dsize);
				loc->nlnum=i;
				loc->off=off;
				loc->hub=hub;
				memcpy(loc+1,oldloc,dat.dsize);
				dat.dsize+=sizeof(struct _loc);
			}
			else
			{
				loc=(struct _loc *)xmalloc(sizeof(struct _loc));
				loc->nlnum=i;
				loc->off=off;
				loc->hub=hub;
				dat.dsize=sizeof(struct _loc);
			}
			dat.dptr=(char *)loc;
			debug(21,"store: [%d] %s",dat.dsize,
				printable(dat.dptr,dat.dsize));
#ifdef HAS_NDBM_H
			if (dbm_store(nldb,key,dat,DBM_REPLACE) < 0)
#else
			if (store(key,dat))
#endif
			{
				logerr("cannot store %u:%u/%u.%u as (%u,%lu)",
					addr.zone,addr.net,addr.node,
					addr.point,i,(unsigned long)off);
			}
			free(loc);
			entries++;
			total++;
		}
		loginf("%d entries in nodelist \"%s[.???]\"",
			entries,S(tmp->addr->name));
	}
#ifndef DONT_HAVE_DBMCLOSE
#ifdef HAS_NDBM_H
	dbm_close(nldb);
#else
	dbmclose();
#endif
#endif
	loginf("Total %d entries in nodelist index",total);

	return rc;
}

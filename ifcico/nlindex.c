#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include "directory.h"
#include <sys/stat.h>
#include "xutil.h"
#include "lutil.h"
#include "ftn.h"
#include "config.h"
#include "nodelist.h"
#include "nlindex.h"

#ifdef HAS_NDBM_H
DBM *nldb=NULL;
#endif
int openstatus = 0;
int n;

DIR *dp;
struct dirent *de;

struct _nodelist *nodevector;

struct _pkey pkey[] = {
	{"Down",NL_NODE,NL_DOWN},
	{"Hold",NL_NODE,NL_HOLD},
	{"Host",NL_NET,0},
	{"Hub",NL_NODE,NL_HUB},
	{"Point",NL_POINT,0},
	{"Pvt",NL_NODE,NL_PVT},
	{"Region",NL_NET,NL_REGION},
	{"Zone",NL_ZONE,0},
	{NULL,0,0}
};

struct _fkey fkey[] = {
	{"CM",CM},
	{"MO",MO},
	{"LO",LO},
	{"V21",V21},
	{"V22",V22},
	{"V29",V29},
	{"V32",V32},
	{"V32B",V32B|V32},
	{"V33",V33},
	{"V34",V34},
	{"V42",V42|MNP},
	{"V42B",V42B|V42|MNP},
	{"MNP",MNP},
	{"H96",H96},
	{"HST",HST|MNP},
	{"H14",H14|HST|MNP},
	{"H16",H16|H14|HST|MNP|V42B},
	{"MAX",MAX},
	{"PEP",PEP},
	{"CSP",CSP},
	{"ZYX",ZYX|V32B|V32|V42B|V42|MNP},
	{"MN",MN},
	{"XA",XA},
	{"XB",XB},
	{"XC",XC},
	{"XP",XP},
	{"XR",XR},
	{"XW",XW},
	{"XX",XX},
	{NULL,0}
};

int initnl(void)
{
	int i,rc;
	fa_list *tmp;
	time_t lastmtime;
	struct stat stbuf;
	char *indexnm,*nlnm=NULL,*p;

	if (openstatus > 1) return openstatus;
	if (openstatus == 1) return 0;

	n=0;
	for (tmp=nllist;tmp;tmp=tmp->next) n++;
	debug(20,"Initialize %d nodelists",n);

	nodevector=(struct _nodelist *)xmalloc(n * sizeof(struct _nodelist));

	lastmtime=configtime;
	i=0;
	for (tmp=nllist;tmp;tmp=tmp->next)
	{
		if (tmp->addr->domain)
			nodevector[i].domain=tmp->addr->domain;
		else if (whoami->addr->domain)
			nodevector[i].domain=whoami->addr->domain;
		else
			nodevector[i].domain="fidonet";

		nlnm=xstrcpy(tmp->addr->name);
		if ((rc=stat(tmp->addr->name,&stbuf)) != 0)
		{
			int next,mext=0;

			if (nlnm) free(nlnm);
			if ((nlnm=strrchr(tmp->addr->name,'/')))
				nlnm++;
			else nlnm=tmp->addr->name;
			if (dp == NULL) dp=opendir(nlbase);
			if (dp != NULL)
			{
				rewinddir(dp);
				while ((de=readdir(dp)))
				if (strncmp(de->d_name,nlnm,strlen(nlnm)) == 0)
				{
					p=(de->d_name)+strlen(nlnm);
					if ((*p == '.') && (strlen(p) == 4) &&
					    (strspn(p+1,"0123456789") == 3))
					{
						next=atoi(p+1);
						if (next > mext) mext=next;
					}
				}
			}
			else logerr("$cannot open \"%s\" directory",S(nlbase));
			nlnm=xstrcpy(tmp->addr->name);
			nlnm=xstrcat(nlnm,".999");
			sprintf(nlnm+strlen(nlnm)-3,"%03d",mext);
			debug(20,"try \"%s\" nodelist",S(nlnm));
			rc=stat(nlnm,&stbuf);
		}
		if (rc == 0)
		{
			if (stbuf.st_mtime > lastmtime) 
				lastmtime=stbuf.st_mtime;
			nodevector[i].fp=fopen(nlnm,"r");
			if (nodevector[i].fp == NULL)
				logerr("$cannot open nodelist \"%s\"",S(nlnm));
			else debug(20,"opened nodelist \"%s\"",S(nlnm));
		}
		else
		{
			logerr("$cannot stat nodelist \"%s\"",
				S(tmp->addr->name));
			nodevector[i].fp=NULL;
		}
		i++;
		if (nlnm) free(nlnm);
	}

	if (dp != NULL) closedir(dp);
	dp=NULL;

	for (i=0;i<n;i++)
	{
		debug(20,"nodelist: %3d: %-08s %08lx",
			i,nodevector[i].domain,nodevector[i].fp);
	}

	indexnm=xstrcpy(nlbase);
	indexnm=xstrcat(indexnm,INDEX);
#ifdef HAS_BSD_DB
	indexnm=xstrcat(indexnm,".db");
#else
	indexnm=xstrcat(indexnm,".dir");
#endif
	if ((stat(indexnm,&stbuf) != 0) || (stbuf.st_mtime < lastmtime)) rc=1;
	else rc=0;

#ifdef HAS_BSD_DB
	indexnm[strlen(indexnm)-3]='\0';
#else
	indexnm[strlen(indexnm)-4]='\0';
#endif
	debug(20,"Trying open existing \"%s\"",S(indexnm));
#ifdef HAS_NDBM_H
	if (nldb == NULL)
	if ((nldb=dbm_open(indexnm,O_RDONLY,0600)) == NULL)
		rc=2;
#else
	if (dbminit(indexnm) != 0) rc=2;
#endif

	free(indexnm);

	if (rc > 1)
	{
		openstatus=2;
		logerr("$cannot open nodelist index (%d)",rc);
	}
	else openstatus=1;

	return rc;
}

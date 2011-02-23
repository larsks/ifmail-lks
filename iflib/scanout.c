#include <stdlib.h>
#include <stdio.h>
#include "directory.h"
#include <string.h>
#include <ctype.h>
#include "xutil.h"
#include "config.h"
#include "ftn.h"
#include "scanout.h"
#include "lutil.h"

#ifndef PATH_MAX
#define PATH_MAX 512
#endif

extern int fakeoutbound;
#define FAKEOUT "/tmp/ifmail/"

typedef struct _strl {
	struct _strl *next;
	char *str;
	char *dom;
	int zone;
} strl;

static faddr addr = {
	NULL,
	0,0,0,0,
	NULL
};

static int scan_dir(int (*)(faddr*,char,int,char*),char*,int);
static int scan_dir(fn,dname,ispoint)
int (*fn)(faddr*,char,int,char*);
char *dname;
int ispoint;
{
	int rc=0;
	char fname[PATH_MAX];
	char flavor='?';
	DIR *dp=NULL;
	struct dirent *de;
	int isflo;

	debug(7,"enter scan_dir \"%s\" (%s)",S(dname),ispoint?"point":"node");

	if ((dp=opendir(dname)) == NULL)
	{
		debug(7,"outbound \"%s\" cannot be opened, proceed",S(dname));
		return 0;
	}

	while ((de=readdir(dp)))
	if ((strlen(de->d_name) == 12) &&
	    (de->d_name[8] == '.') &&
	    (strspn(de->d_name,"0123456789abcdefABCDEF") == 8))
	{
		debug(7,"checking: \"%s\"",de->d_name);
		addr.point=0;
		strncpy(fname,dname,PATH_MAX-2);
		strcat(fname,"/");
		strncat(fname,de->d_name,PATH_MAX-strlen(fname)-2);
		if ((strcasecmp(de->d_name+9,"pnt") == 0) && !ispoint)
		{
			sscanf(de->d_name,"%04x%04x",&addr.net,&addr.node);
			if ((rc=scan_dir(fn,fname,1))) goto exout;
		}
		else if ((strcasecmp(de->d_name+8,".out") == 0) ||
			 (strcasecmp(de->d_name+8,".cut") == 0) ||
			 (strcasecmp(de->d_name+8,".hut") == 0) ||
			 (strcasecmp(de->d_name+8,".opk") == 0) ||
			 (strcasecmp(de->d_name+8,".cpk") == 0) ||
			 (strcasecmp(de->d_name+8,".hpk") == 0) ||
			 (strcasecmp(de->d_name+8,".flo") == 0) ||
			 (strcasecmp(de->d_name+8,".clo") == 0) ||
			 (strcasecmp(de->d_name+8,".hlo") == 0) ||
			 (strcasecmp(de->d_name+8,".req") == 0))
		{
			if (ispoint)
				sscanf(de->d_name,"%08x",
					&addr.point);
			else
				sscanf(de->d_name,"%04x%04x",
					&addr.net,&addr.node);
			flavor=tolower(de->d_name[9]);
			if (flavor == 'f') flavor='o';
			if (strcasecmp(de->d_name+10,"ut") == 0)
				isflo=OUT_PKT;
			else if (strcasecmp(de->d_name+10,"pk") == 0)
				isflo=OUT_DIR;
			else if (strcasecmp(de->d_name+10,"lo") == 0)
				isflo=OUT_FLO;
			else if (strcasecmp(de->d_name+9,"req") == 0)
				isflo=OUT_REQ;
			else
				isflo=-1;
			debug(7,"%s \"%s\"",
				(isflo == OUT_FLO) ? "flo file" : "packet",
				de->d_name);
			if ((rc=fn(&addr,flavor,isflo,fname))) goto exout;
		}
		else if ((strncasecmp(de->d_name+9,"su",2) == 0) ||
		         (strncasecmp(de->d_name+9,"mo",2) == 0) ||
		         (strncasecmp(de->d_name+9,"tu",2) == 0) ||
		         (strncasecmp(de->d_name+9,"we",2) == 0) ||
		         (strncasecmp(de->d_name+9,"th",2) == 0) ||
		         (strncasecmp(de->d_name+9,"fr",2) == 0) ||
		         (strncasecmp(de->d_name+9,"sa",2) == 0))
		{
			isflo=OUT_ARC;
			if ((rc=fn(&addr,flavor,isflo,fname))) goto exout;
			debug(7,"arcmail file \"%s\"",de->d_name);
		}
		else
		{
			debug(7,"skipping \"%s\"",de->d_name);
		}
	}

exout:
	closedir(dp);
	return rc;
}

int scanout(fn)
int (*fn)(faddr*,char,int,char*);
{
	int rc=0;
	char fname[PATH_MAX];
	fa_list *dom;
	strl *doml=NULL,*tmp,**tmpd;
	char *p,*q;
	DIR *dp;
	struct dirent *de;

	if (fakeoutbound)
	{
		strcpy(fname,FAKEOUT);
		if (whoami->addr->domain)
			strcat(fname,whoami->addr->domain);
		else
			strcat(fname,"fidonet");
	}
	else
	{
		strncpy(fname,outbound,PATH_MAX-1);
	}
	if ((p=strrchr(fname+1,'/'))) *p='\0';
	else fname[0]='\0';

	if ((dp=opendir(fname)) == NULL)
	{
		logerr("$cannot open outbound directory \"%s\" for reading",
			S(fname));
		return 1;
	}

	for (dom=nllist;dom;dom=dom->next)
	{
		debug(7,"checking domain %s",S(dom->addr->domain));
		if ((p=dom->addr->domain) == NULL) p=whoami->addr->domain;
		q=p;
		if (((p == NULL) || (whoami->addr->domain == NULL) ||
		     (strcasecmp(p,whoami->addr->domain) == 0)) &&
		    (!fakeoutbound))
		{
			if (dom->addr->zone == 0)
				dom->addr->zone=whoami->addr->zone;
			if ((p=strrchr(outbound,'/'))) p++;
			else p=outbound;
		}
		for (tmpd=&doml;*tmpd;tmpd=&((*tmpd)->next))
			if (strcasecmp(p,(*tmpd)->str) == 0) break;
		if (*tmpd == NULL)
		{
			*tmpd=(strl*)xmalloc(sizeof(strl));
			(*tmpd)->next=NULL;
			(*tmpd)->str=p;
			(*tmpd)->dom=q;
			(*tmpd)->zone=dom->addr->zone;
		}
	}

	while ((de=readdir(dp)))
	for (tmpd=&doml;*tmpd;tmpd=&((*tmpd)->next))
	{
		debug(7,"checking directory %s against %s",
			de->d_name,S((*tmpd)->str));
		if ((strncasecmp(de->d_name,(*tmpd)->str,strlen((*tmpd)->str)) == 0) &&
		    ((*(p=de->d_name+strlen((*tmpd)->str)) == '\0') ||
		     ((*p == '.') && (strlen(p+1) == 3) && 
		      (strspn(p+1,"01234567890abcdefABCDEF") == 3))))
		{
			if (*p) sscanf(p+1,"%03x",&addr.zone);
			else addr.zone=(*tmpd)->zone;
			if (fakeoutbound)
			{
				strncpy(fname,FAKEOUT,PATH_MAX-2);
			}
			else
			{
				strncpy(fname,outbound,PATH_MAX-2);
			}
			if ((p=strrchr(fname,'/'))) *p='\0';
			else p=fname+strlen(fname);
			strcat(fname,"/");
			strncat(fname,de->d_name,PATH_MAX-strlen(fname)-2);
			addr.domain=(*tmpd)->dom;
			if ((rc=scan_dir(fn,fname,0))) goto exout;
		}
		else
		{
			debug(7,"skipping \"%s\"",de->d_name);
		}
	}

exout:
	closedir(dp);
	for (tmp=doml;tmp;tmp=doml)
	{
		doml=tmp->next;
		free(tmp);
	}
	return rc;
}

#ifdef TESTING

extern char *configname;
extern int readconfig();

int each(faddr*,char,int,char*);
int each(addr,flavor,isflo,name)
faddr *addr;
char flavor;
int isflo;
char *name;
{
	debug(0,"addr \"%s\" flavor '%c', %s, name \"%s\"",
		ascfnode(addr,0xff),flavor,isflo?"flo":"pkt",name);
	return 0;
}

int main(argc,argv)
int argc;
char *argv[];
{
	int rc;

	if (argv[1]) configname=argv[1];
	else configname="config";
	if ((rc=readconfig()))
	{
		debug(0,"readconfig failed - %d",rc);
		return rc;
	}
	verbose=1<<6;

	rc=scanout(each);
	debug(0,"scanout rc=%d",rc);
	return rc;
}

#endif

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include <ctype.h>
#include <errno.h>
#include "config.h"
#include "xutil.h"
#include "lutil.h"
#include "bwrite.h"
#include "session.h"
#include "ftn.h"

#ifndef PATH_MAX
#define PATH_MAX 512
#endif

#define LEAVE 0
#define TFS 1
#define KFS 2
#define DSF 3

extern int master;
int made_request;

extern FILE *openpkt(FILE *,faddr*,char);
extern char *pktname(faddr*,char);
extern char *floname(faddr*,char);
extern char *reqname(faddr*,char);
extern char *splname(faddr*,char);
extern unsigned INT32 sequencer(void);

static char *tmpkname(void);
static char *tmpkname(void)
{
	static char buf[16];
	sprintf(buf,"%08lx.pkt",(unsigned long)sequencer());
	return buf;
}

static char *xtodos(char*); /* return a _new_copy_ like xstrcpy() */
static char *xtodos(orig)
char *orig;
{
	int i;
	char buf[8+1+3+1],*copy,*p,*q,*r;

	if (orig == NULL) return NULL;

	if ((remote_flags & SESSION_FNC) == 0)
	{
		debug(12,"No filename conversion for \"%s\"",S(orig));
		return xstrcpy(orig);
	}

	copy=xstrcpy(orig);
	if ((p=strrchr(copy,'/'))) p++;
	else p=copy;

	if (strcmp(q=copy+strlen(copy)-strlen(".tar.gz"),".tar.gz") == 0)
	{
		*q='\0';
		q="tgz";
	}
	else if (strcmp(q=copy+strlen(copy)-strlen(".tar.z"),".tar.z") == 0)
	{
		*q='\0';
		q="tgz";
	}
	else if (strcmp(q=copy+strlen(copy)-strlen(".tar.Z"),".tar.Z") == 0)
	{
		*q='\0';
		q="taz";
	}
	else if ((q=strrchr(p,'.'))) *q++='\0';
	else q=NULL;

	r=buf;
	for (i=0;(i<8) && (*p);i++,p++,r++)
	switch (*p)
	{
	case '.':
	case '\\':	*r='_'; break;
	default:	*r=toupper(*p);
	}
	if (q)
	{
		*r++='.';
		for (i=0;(i<3) && (*q);i++,q++,r++)
		switch (*q)
		{
		case '.':
		case '\\':	*r='_'; break;
		default:	*r=toupper(*q);
		}
	}
	*r++='\0';

	debug(12,"name \"%s\" converted to \"%s\"",S(orig),S(buf));

	free(copy);
	return xstrcpy(buf);
}

void add_list(file_list**,char*,char*,int,off_t,FILE*,int);
void add_list(lst,local,remote,disposition,floff,flofp,toend)
file_list **lst;
char *local;
char *remote;
int disposition;
off_t floff;
FILE *flofp;
int toend;
{
	file_list **tmpl;
	file_list *tmp;

	debug(12,"add_list(\"%s\",\"%s\",%d,%s)",
		S(local),S(remote),disposition,toend?"to end":"to beg");
	if (toend) for (tmpl=lst;*tmpl;tmpl=&((*tmpl)->next));
	else tmpl=&tmp;
	*tmpl=(file_list*)xmalloc(sizeof(file_list));
	if (toend)
	{
		(*tmpl)->next=NULL;
	}
	else
	{
		(*tmpl)->next=*lst;
		*lst=*tmpl;
	}
	(*tmpl)->local=xstrcpy(local);
	(*tmpl)->remote=xtodos(remote);
	(*tmpl)->disposition=disposition;
	(*tmpl)->floff=floff;
	(*tmpl)->flofp=flofp;
	return;
}

static void check_flo(file_list**,char*);
static void check_flo(lst,nm)
file_list **lst;
char *nm;
{
	FILE *fp;
	off_t off;
	struct flock fl;
	char buf[PATH_MAX],buf2[PATH_MAX],*p,*q;
	int disposition;
	struct stat stbuf;

	debug(12,"check_flo(\"%s\")",S(nm));
	if ((fp=fopen(nm,"r+")) == NULL)
	{
		debug(12,"no flo file");
		return;
	}
	fl.l_type=F_RDLCK;
	fl.l_whence=0;
	fl.l_start=0L;
	fl.l_len=0L;
	if (fcntl(fileno(fp),F_SETLK,&fl) != 0)
	{
		if (errno != EAGAIN)
			logerr("$cannot read-lock \"%s\"",S(nm));
		else debug(12,"flo file busy");
		fclose(fp);
		return;
	}
	if (stat(nm,&stbuf) != 0)
	{
		logerr("$cannot access \"%s\"",S(nm));
		fclose(fp);
		return;
	}

	while (!feof(fp) && !ferror(fp))
	{
		off=ftell(fp);
		if (fgets(buf,sizeof(buf)-1,fp) == NULL) continue;
		if (buf[0] == '~') continue; /* skip sent files */
		if (*(p=buf+strlen(buf)-1) == '\n') *p--='\0';
		if (*p == '\r') *p='\0';
		switch (buf[0])
		{
		case '#':	p=buf+1; disposition=TFS; break;
		case '-':
		case '^':	p=buf+1; disposition=KFS; break;
		case '@':	p=buf+1; disposition=LEAVE; break;
		case 0:		continue;
		default:	p=buf; disposition=LEAVE; break;
		}
		if (dosoutbound &&
			(strncasecmp(p,dosoutbound,strlen(dosoutbound)) == 0))
		{
			strcpy(buf2,outbound);
			for (p+=strlen(dosoutbound),q=buf2+strlen(buf2);
								*p;p++,q++)
				*q=((*p) == '\\')?'/':tolower(*p);
			*q='\0';
			p=buf2;
		}
		if ((q=strrchr(p,'/'))) q++;
		else q=p;
		add_list(lst,p,q,disposition,off,fp,1);
	}

	add_list(lst,nm,NULL,KFS,0L,fp,1);

	/* here, we leave .flo file open, it will be closed by */
	/* execute_disposition */

	return;
}

file_list *create_filelist(al,fl,create)
fa_list *al;
char *fl;
int create;
{
	file_list *st=NULL,*tmpf;
	fa_list *tmpa;
	char flavor;
	char *nm;
	char tmpreq[13];
	struct stat stbuf;
	int packets=0;
	FILE *fp;

	debug(12,"create_filelist(%s,\"%s\",%d)",
		al?ascfnode(al->addr,0x1f):"<none>",
		S(fl),create);
	made_request=0;
	while ((flavor=*fl++))
	for (tmpa=al;tmpa;tmpa=tmpa->next)
	{
		if (flavor == 'o')
		{
			nm=splname(tmpa->addr,flavor);
			if ((fp=fopen(nm,"w"))) fclose(fp);
			if ((nm != NULL) && (stat(nm,&stbuf) == 0))
			{
				add_list(&st,nm,NULL,DSF,0L,NULL,1);
			}
		}

		nm=pktname(tmpa->addr,flavor);
		if ((nm != NULL) && (stat(nm,&stbuf) == 0))
		{
			packets++;
			add_list(&st,nm,tmpkname(),KFS,0L,NULL,1);
		}

		nm=floname(tmpa->addr,flavor);
		check_flo(&st,nm);

		if ((session_flags & SESSION_WAZOO) &&
		    (master || ((session_flags & SESSION_IFNA) == 0)))
		if (flavor == 'o') /* we don't distinguish flavoured reqs */
		{
			nm=reqname(tmpa->addr,flavor);
			if ((nm != NULL) && (stat(nm,&stbuf) == 0))
			{
				sprintf(tmpreq,"%04X%04X.REQ",
					tmpa->addr->net,tmpa->addr->node);
				add_list(&st,nm,tmpreq,DSF,0L,NULL,1);
				made_request=1;
			}
		}
	}

	if (((st == NULL) && (create > 1)) ||
	    ((st != NULL) && (packets == 0) && (create > 0)))
	{
		debug(12,"create packet for %s",ascfnode(al->addr,0x1f));
		if ((fp=openpkt(NULL,al->addr,'o')))
		{
			iwrite(0,fp);
			fclose(fp);
		}
		add_list(&st,pktname(al->addr,'o'),tmpkname(),KFS,0L,NULL,0);
	}

	for (tmpf=st;tmpf;tmpf=tmpf->next)
	debug(12,"flist: \"%s\" -> \"%s\" dsp:%d flofp:%lu floff:%lu",
		S(tmpf->local),S(tmpf->remote),tmpf->disposition,
		(unsigned long)tmpf->flofp,(unsigned long)tmpf->floff);

	return st;
}

void tidy_filelist(fl,dsf)
file_list *fl;
int dsf;
{
	file_list *tmp;

	for (tmp=fl;fl;fl=tmp)
	{
		tmp=fl->next;
		if (dsf && (fl->disposition == DSF))
		{
			debug(12,"removing sent file \"%s\"",S(fl->local));
			if (unlink(fl->local) != 0)
				if (errno == ENOENT)
					debug(12,"cannot unlink nonexistent file \"%s\"",
						S(fl->local));
				else
					logerr("$cannot unlink file \"%s\"",
						S(fl->local));
		}
		if (fl->local) free(fl->local);
		if (fl->remote) free(fl->remote);
		else if (fl->flofp) fclose(fl->flofp);
		free(fl);
	}
	return;
}

void execute_disposition(fl)
file_list *fl;
{
	FILE *fp=NULL;
	char *nm;
	char tpl='~';

	nm=fl->local;
	if ((fl->flofp) && (fl->floff != -1))
	{
		if (fseek(fl->flofp,fl->floff,0) == 0)
		{
			if (fwrite(&tpl,1,1,fl->flofp) != 1)
				logerr("$error writing '~' to .flo at %lu",
					(unsigned long)fl->floff);
			fflush(fl->flofp);
#ifdef HAS_FSYNC
			fsync(fileno(fl->flofp));
#endif
		}
		else logerr("$error seeking in .flo to %lu",
			(unsigned long)fl->floff);
	}
	switch (fl->disposition)
	{
	case DSF:
	case LEAVE:	debug(12,"leaving sent file \"%s\"",S(nm));
			break;
	case TFS:	debug(12,"truncating sent file \"%s\"",S(nm));
			if ((fp=fopen(nm,"w"))) fclose(fp);
			else logerr("$cannot truncate file \"%s\"",S(nm));
			break;
	case KFS:	debug(12,"removing sent file \"%s\"",S(nm));
			if (unlink(nm) != 0)
				if (errno == ENOENT)
					debug(12,"cannot unlink nonexistent file \"%s\"",
						S(nm));
				else
					logerr("$cannot unlink file \"%s\"",
						S(nm));
			break;
	default:	logerr("execute_disposition: unknown disp %d for \"%s\"",
				fl->disposition,S(nm));
			break;
	}
	return;
}

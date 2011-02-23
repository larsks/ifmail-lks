#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "lutil.h"
#include "xutil.h"
#include "areas.h"
#include "config.h"
#include "lhash.h"

typedef struct _areang {
	char *area;
	char *ng;
	char *dist;
} areang;

extern area_list *badgroups;
static LHASH *nglist_byarea=(LHASH*)0;
static LHASH *nglist_byng=(LHASH*)0;
char *defgroup=NULL,*defdist=NULL,*defarea=NULL;
int group_count;

static char *expand(p,q)
char *p,*q;
{
	char *r,*tmp,*ret;

	debug(9,"expanding \"%s\" + \"%s\"",S(p),S(q));

	tmp=xstrcpy(p);
	if ((r=strchr(tmp,'*'))) *r++='\0';
	ret=xstrcpy(tmp);
	if (r)
	{
		ret=xstrcat(ret,q);
		ret=xstrcat(ret,r);
	}
	free(tmp);
	debug(9,"expand result \"%s\"",S(ret));
	return ret;
}

static unsigned long hash_byarea(hash_entry)
areang *hash_entry;
{
	return lh_strhash(hash_entry->area);
}

static unsigned long hash_byng(hash_entry)
areang *hash_entry;
{
	return lh_strhash(hash_entry->ng);
}

static int cmp_byarea(e1,e2)
areang *e1,*e2;
{
	return strcmp(e1->area,e2->area);
}

static int cmp_byng(e1,e2)
areang *e1,*e2;
{
	return strcmp(e1->ng,e2->ng);
}

void tidy_arealist(al)
area_list *al;
{
	area_list *tmp;

	for (;al;al=tmp)
	{
		tmp=al->next;
		if (al->name) free(al->name);
		free(al);
	}
}

area_list *areas(ngroups)
char *ngroups;
{
	char *ng,*p,*q,*r=NULL;
	area_list *start=NULL,**cur,*bg;
	areang *ngl;
	areang dummy_entry;
	int forbid=0;

	if (ngroups == NULL) return NULL;
	ng=xstrcpy(ngroups);
	cur=&start;

	group_count=0;
	for (p=strtok(ng," ,\t\n");p;p=strtok(NULL," ,\t\n"))
	{
		group_count++;
		q=NULL;
		for (bg=badgroups;bg;bg=bg->next)
		if (!strncmp(bg->name,p,strlen(bg->name)))
		{
			loginf("Message not gated due to forbidden group %s",
				S(p));
			forbid=1;
			break;
		}
		dummy_entry.ng=p;
		if ((ngl=(areang*)lh_retrieve(nglist_byng,(char*)&dummy_entry)))
		{
			q=ngl->area;
		}
		else q=NULL;
		if (q || defarea)
		{
			if (q == NULL)
			{
				for (r=p;*r;r++) *r=toupper(*r);
				q=expand(defarea,p);
			}
			debug(9,"newsgroup \"%s\" -> area \"%s\"",S(p),S(q));
			(*cur)=(area_list *)xmalloc(sizeof(area_list));
			(*cur)->next=NULL;
			(*cur)->name=xstrcpy(q);
			cur=&((*cur)->next);
		}
	}

	free(ng);
	if (forbid)
	{
		tidy_arealist(start);
		return NULL;
	}
	else return start;
}

void ngdist(area,newsgroup,distribution)
char *area,**newsgroup,**distribution;
{
	areang *ang;
	areang dummy_entry;
	char *p,*q;

	*newsgroup=NULL;
	*distribution=NULL;
	p=xstrcpy(area);
	if ((*(q=p+strlen(p)-1) == '\r') || (*q == '\n')) *q='\0';

	dummy_entry.area=p;
	if ((ang=(areang*)lh_retrieve(nglist_byarea,(char*)&dummy_entry)))
	{
		*newsgroup=ang->ng;
		*distribution=ang->dist;
		debug(9,"area \"%s\" -> newsgroup \"%s\", dist \"%s\"",
			S(p),S(*newsgroup),S(*distribution));
	}
	else
	{
		*newsgroup = NULL;
		*distribution = NULL;
	}

	if ((*newsgroup == NULL) && defgroup)
	{
		for (q=p;*q;q++) *q=tolower(*q);
		*newsgroup=expand(defgroup,p);
		*distribution=defdist;
		debug(9,"area \"%s\" -> default newsgroup \"%s\", dist \"%s\"",
			S(p),S(*newsgroup),S(*distribution));
	}

	if (*newsgroup == NULL)
	{
		loginf("No newsgroup for area tag %s",S(p));
	}

	free(p);
}

void readareas(fn)
char *fn;
{
	FILE *fp;
	char buf[128],*p,*q,*r;
	areang *cur;

	if (defgroup) free(defgroup);
	defgroup=NULL;
	if (defdist) free(defdist);
	defdist=NULL;
	if (defarea) free(defarea);
	defarea=NULL;

	if (!nglist_byarea) nglist_byarea=lh_new(hash_byarea,cmp_byarea);
	if (!nglist_byng) nglist_byng=lh_new(hash_byng,cmp_byng);

	if ((fp=fopen(fn,"r")) == NULL)
	{
		logerr("$unable to open area file %s",S(fn));
		return;
	}
	while (fgets(buf,sizeof(buf)-1,fp))
	{
		if (isspace(buf[0])) continue;
		p=strtok(buf," \t\n");
		q=strtok(NULL," \t\n");
		r=strtok(NULL," \t\n");
		if (p && q)
		{
			if (strcmp(q,"*") == 0)
			{
				debug(9,"adding default areatag \"%s\"",S(p));
				defarea=xstrcpy(p);
			}
			if (strcmp(p,"*") == 0)
			{
				debug(9,"adding default group \"%s\" (dist %s)",
					S(q),S(r));
				defgroup=xstrcpy(q);
				defdist=xstrcpy(r);
			}
			if ((strcmp(p,"*") != 0) && (strcmp(q,"*") != 0))
			{
				debug(9,"adding area \"%s\" for ng \"%s\" (dist %s)",
					S(p),S(q),S(r));
				cur=(areang*)malloc(sizeof(areang));
				cur->area=xstrcpy(p);
				cur->ng=xstrcpy(q);
				if (r) cur->dist=xstrcpy(r);
				else cur->dist=NULL;
				lh_insert(nglist_byarea,(char*)cur);
				lh_insert(nglist_byng,(char*)cur);
			}
		}
	}
	fclose(fp);
}

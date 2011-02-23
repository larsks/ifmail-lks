#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "xutil.h"
#include "lutil.h"
#include "ftn.h"
#include "ftnmsg.h"
#include "rfcmsg.h"
#include "config.h"
#include "version.h"
#include "hash.h"

#ifndef ULONG_MAX
#define ULONG_MAX 4294967295
#endif

int newsmode=0;
char *replyaddr=NULL;

extern time_t now;
extern faddr *bestaka;

extern time_t parsedate(char *,void *);
extern int registrate(char *,char *);
extern unsigned long atoul(char*);

extern unsigned INT16 crc(char *);
extern int flagset(char *);

int ftnmsgid(msgid,s,n)
char *msgid;
char **s;
unsigned long *n;
{
	char *buf,*l,*r,*p;
	unsigned long nid=0L;
	faddr *tmp;
	int ftnorigin=0;

	if (msgid == NULL)
	{
		*s=NULL;
		*n=0L;
		return ftnorigin;
	}

	buf=xmalloc(strlen(msgid)+65);
	strcpy(buf,msgid);
	if ((l=strchr(buf,'<'))) l++;
	else l=buf;
	while (isspace(*l)) l++;
	if ((r=strchr(l,'>'))) *r='\0';
	r=l+strlen(l)-1;
	while (isspace(*r) && (r > l)) (*r--)='\0';
	if ((tmp=parsefaddr(l)))
	{
		if (tmp->name)
		{
			if (strspn(tmp->name,"0123456789") == strlen(tmp->name))
				nid=atoul(tmp->name);
			else nid=ULONG_MAX;
			if (nid == ULONG_MAX)
				hash_update_s(&nid,tmp->name);
			else
				ftnorigin=1;
		}
		else
		{
			hash_update_s(&nid,l);
		}
		*s=xstrcpy(ascfnode(tmp,0x1f));
		tidy_faddr(tmp);
	}
	else
	{
		if ((r=strchr(l,'@')) == NULL) /* should never happen */
		{
			*s=xstrcpy(l);
			hash_update_s(&nid,l);
		}
		else
		{
			*r='\0';
			if (p=strchr(l,'%'))
			{
				*p='\0';
				if (strspn(l,"0123456789") == strlen(l))
				{
					*r='@';
					r=p;
				}
				else *p='%';
			}
			r++;
			if (strspn(l,"0123456789") == strlen(l))
				nid=atoul(l);
			else nid=ULONG_MAX;
			if (nid == ULONG_MAX) {
				hash_update_s(&nid,l);
			}
			if (strchr(r,'@'))
			{
				*s=xmalloc(strlen(r)+3);
				sprintf(*s,"<%s>",r);
			}
			else *s=xstrcpy(r);
		}
	}
	*n=nid;

	free(buf);
	return ftnorigin;
}

ftnmsg *mkftnhdr(msg)
rfcmsg *msg;
{
	char *freename,*rfcfrom,*p,*l,*r;
	char *fbuf=NULL;
	ftnmsg *tmsg;
	int needreplyaddr;

	tmsg=(ftnmsg *)xmalloc(sizeof(ftnmsg));

	tmsg->flags=0;
	tmsg->ftnorigin=0;
	tmsg->to=NULL;
	tmsg->from=NULL;
	tmsg->date=0L;
	tmsg->subj=NULL;
	tmsg->msgid_s=NULL;
	tmsg->msgid_a=NULL;
	tmsg->reply_s=NULL;
	tmsg->reply_a=NULL;
	tmsg->origin=NULL;
	tmsg->area=NULL;

	if (newsmode)
	{
		p=xstrcpy(hdr("Comment-To",msg));
		if (p == NULL) p=xstrcpy(hdr("X-Comment-To",msg));
		if (p == NULL) p=xstrcpy(hdr("X-FTN-To",msg));
		if (p == NULL) p=xstrcpy(hdr("X-Fidonet-Comment-To",msg));
		if (p == NULL) p=xstrcpy(hdr("X-Apparently-To",msg));
		if (p)
		{
			debug(6,"getting `to' address from: \"%s\"",p);
			if ((tmsg->to=parsefaddr(p)) == NULL)
			{
				tmsg->to=parsefaddr("All@p0.f0.n0.z0");
				if ((l=strrchr(p,'<')) && (r=strchr(p,'>')) && (l < r))
				{
					r=l;
					*r--='\0';
					while (isspace(*r)) *r--='\0';
					l=p;
					while (isspace(*l)) l++;
				}
				else if ((l=strrchr(p,'(')) && (r=strchr(p,')')) && 
				         (l < r))
				{
					*r--='\0';
					while (isspace(*r)) *r--='\0';
					l++;
					while (isspace(*l)) l++;
				}
				else
				{
					l=p;
					while (isspace(*l)) l++;
					r=p+strlen(p)-1;
					if (*r == '\n') *r--='\0';
					while (isspace(*r)) *r--='\0';
				}

				if(*l)
				{
					if (strlen(l) > MAXNAME)
						l[MAXNAME]='\0';
					free(tmsg->to->name);
					tmsg->to->name=xstrcpy(l);
				}
			}
			free(p);
		}
		else
		tmsg->to=parsefaddr("All@p0.f0.n0.z0");
		debug(3,"TO: %s",ascinode(tmsg->to,0x7f));
	}

	p=fbuf=xstrcpy(hdr("From",msg));
	if (fbuf == NULL)
	{
		p=fbuf=xstrcpy(hdr("X-UUCP-From",msg));
		if (p) 
		{
			while (isspace(*p)) p++;
			if ((l=strchr(p,' '))) *l='\0'; 
		}
	}
	if (fbuf == NULL) p=fbuf=xstrcpy("Unidentified User <postmaster>");
	if ((l=strchr(p,'<')) && (r=strchr(p,'>')) && (l < r))
	{
		*l++ = '\0';
		*r++ = '\0';
		rfcfrom=l;
		freename=p;
		debug(6,"From address: \"%s\", name: \"%s\" (angle brackets)",
			rfcfrom,freename);
	}
	else if ((l=strchr(p,'(')) && (r=strrchr(p,')')) && (l < r))
	{
		*l++ = '\0';
		*r++ = '\0';
		rfcfrom=p;
		freename=l;
		debug(6,"From address: \"%s\", name: \"%s\" (round brackets)",
			rfcfrom,freename);
	}
	else
	{
		rfcfrom=p;
		freename=p;
		debug(6,"From address: \"%s\", name: \"%s\" (no brackets)",
			rfcfrom,freename);
	}
	if (rfcfrom)
	{
		while (isspace(*rfcfrom)) rfcfrom++;
		p=rfcfrom+strlen(rfcfrom)-1;
		while ((isspace(*p)) || (*p == '\n')) *(p--)='\0';
	}
	if (freename)
	{
		while (isspace(*freename)) freename++;
		p=freename+strlen(freename)-1;
		while ((isspace(*p)) || (*p == '\n')) *(p--)='\0';
		if ((*freename == '\"') && (*(p=freename+strlen(freename)-1) == '\"'))
		{
			freename++;
			*p='\0';
		}
	}
	if (*freename == '\0') freename=rfcfrom;

	if (newsmode) debug(3,"FROM: %s <%s>",freename,rfcfrom);
	else loginf("from: %s <%s>",freename,rfcfrom);

	needreplyaddr=1;
	if ((tmsg->from=parsefaddr(rfcfrom)) == NULL)
	{
		if (freename && rfcfrom)
		if (!strchr(freename,'@') && 
		    !strchr(freename,'%') && 
		    strncasecmp(freename,rfcfrom,MAXNAME) &&
		    strncasecmp(freename,"uucp",4) &&
		    strncasecmp(freename,"usenet",6) &&
		    strncasecmp(freename,"news",4) &&
		    strncasecmp(freename,"super",5) &&
		    strncasecmp(freename,"admin",5) &&
		    strncasecmp(freename,"sys",3)) 
			needreplyaddr=registrate(freename,rfcfrom);
	}
	else
	{
		tmsg->ftnorigin=1;
	}

	if (replyaddr)
	{
		free(replyaddr);
		replyaddr=NULL;
	}
	if (needreplyaddr && (tmsg->from == NULL))
	{
		debug(6,"fill replyaddr with \"%s\"",rfcfrom);
		replyaddr=xstrcpy(rfcfrom);
	}

	debug(6,"From address was%s distinguished as ftn",
		tmsg->from ? "" : " not");

	if ((tmsg->from == NULL) && (bestaka))
	{
		tmsg->from=(faddr *)xmalloc(sizeof(faddr));
		tmsg->from->name=xstrcpy(freename);
		if (tmsg->from->name && (strlen(tmsg->from->name) > MAXNAME))
			tmsg->from->name[MAXNAME]='\0';
		tmsg->from->point=bestaka->point;
		tmsg->from->node=bestaka->node;
		tmsg->from->net=bestaka->net;
		tmsg->from->zone=bestaka->zone;
		tmsg->from->domain=xstrcpy(bestaka->domain);
	}

	if (fbuf) free(fbuf); fbuf=NULL;

	p=hdr("Subject",msg);
	if (p)
	{
		while (isspace(*p)) p++;
		tmsg->subj=xstrcpy(p);
		if (*(p=tmsg->subj+strlen(tmsg->subj)-1) == '\n') *p='\0';
		if (strlen(tmsg->subj) > MAXSUBJ) tmsg->subj[MAXSUBJ]='\0';
	}
	else tmsg->subj=xstrcpy(" ");

	debug(3,"SUBJ: \"%s\"",tmsg->subj);

	if ((p=hdr("X-FTN-FLAGS",msg))) tmsg->flags |= flagset(p);
	if (hdr("Return-Receipt-To",msg)) tmsg->flags |= FLG_RRQ;
	if (!newsmode)
	{
		tmsg->flags |= FLG_PVT;
		tmsg->flags |= FLG_K_S;
	}

	if ((p=hdr("Date",msg))) tmsg->date=parsedate(p,NULL);
	else tmsg->date=time((time_t *)NULL);

	if ((p=hdr("X-FTN-MSGID",msg)))
	{
		tmsg->ftnorigin&=1;
		while (isspace(*p)) p++;
		tmsg->msgid_s=xstrcpy(p);
		if (*(p=tmsg->msgid_s+strlen(tmsg->msgid_s)-1) == '\n') *p='\0';
	}
	else if ((p=hdr("Message-ID",msg)))
	{
		tmsg->ftnorigin &=
			ftnmsgid(p,&(tmsg->msgid_a),&(tmsg->msgid_n));
	}
	else
	{
		tmsg->msgid_a=NULL;
	}

	if ((p=hdr("X-FTN-REPLY",msg)))
	{
		while (isspace(*p)) p++;
		tmsg->reply_s=xstrcpy(p);
		if (*(p=tmsg->reply_s+strlen(tmsg->reply_s)-1) == '\n') *p='\0';
	}
	else if ((p=hdr("In-Reply-To",msg)) == NULL)
	{
		p=hdr("References",msg);
		if (p)
		{
			for (r=p+strlen(p); (*r != ' ') && (r >= p); r--);
			p=r;
		}
	}
	if (p)
	{
		(void)ftnmsgid(p,&(tmsg->reply_a),&(tmsg->reply_n));
	}
	else
	{
		tmsg->reply_a=NULL;
	}

	debug(3,"DATE: %s, MSGID: %s %lu, REPLY: %s %lu",
		ftndate(tmsg->date),
		S(tmsg->msgid_a),tmsg->msgid_n,
		S(tmsg->reply_a),tmsg->reply_n);

	if ((p=hdr("Organization",msg)))
	{
		while (isspace(*p)) p++;
		tmsg->origin=xstrcpy(p);
		if (*(p=tmsg->origin+strlen(tmsg->origin)-1) == '\n') *p='\0';
	}

	debug(3,"ORIGIN: %s",S(tmsg->origin));

	return tmsg;
}

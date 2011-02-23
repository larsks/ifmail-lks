#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "xutil.h"
#include "lutil.h"
#include "ftn.h"
#include "ftnmsg.h"
#include "rfcmsg.h"
#include "config.h"
#include "bwrite.h"
#include "falists.h"
#include "version.h"
#include "hash.h"

#define MAXHDRSIZE 2048
#define MAXSEEN 70
#define MAXPATH 73

extern time_t now;
extern int newsmode;
extern char *replyaddr;
extern faddr *bestaka;

#ifdef HAS_NDBM_H
extern char *idlookup(char *);
#endif
extern unsigned INT32 crc32(char*,int);
extern char *strip_flags(char *);
extern void putbody(int,rfcmsg*,FILE*,FILE*,int,int);

static int removemime;
static int removeorg;
static int removeref;
static int ftnorigin;

static int isftnpath(char*);

int needputrfc(rfcmsg*);
int needputrfc(msg)
rfcmsg *msg;
{
	faddr *ta;

	/* 0-junk, 1-kludge, 2-pass */

	if (!strcasecmp(msg->key,"X-UUCP-From")) return 0;
	if (!strcasecmp(msg->key,"X-Body-Start")) return 0;
	if (!strncasecmp(msg->key,"X-FTN-",6)) return 0;
	if (!strcasecmp(msg->key,"Path")) return isftnpath(msg->val)?0:1;
	if (!strcasecmp(msg->key,"Newsgroups")) return newsmode?0:2;
	if (!strcasecmp(msg->key,"Xref")) return 0;
	if (!strcasecmp(msg->key,"Approved")) return newsmode?0:2;
	if (!strcasecmp(msg->key,"Return-Receipt-To")) return 1;
	if (!strcasecmp(msg->key,"Received")) return newsmode?0:2;
	if (!strcasecmp(msg->key,"From")) return ftnorigin?0:2;
	if (!strcasecmp(msg->key,"To"))
	{
		if (newsmode) return 0;
		if ((ta=parsefaddr(msg->val)))
		{
			tidy_faddr(ta);
			return 0;
		}
		else return 2;
	}
	if (!strcasecmp(msg->key,"Cc")) return 2;
	if (!strcasecmp(msg->key,"Bcc")) return 2;
	if (!strcasecmp(msg->key,"Reply-To")) return 2;
	if (!strcasecmp(msg->key,"Lines")) return 0;
	if (!strcasecmp(msg->key,"Date")) return 0;
	if (!strcasecmp(msg->key,"Subject")) 
	{
		if (strlen(msg->val) > MAXSUBJ) return 2;
		else return 0;
	}
	if (!strcasecmp(msg->key,"Organization")) return removeorg?0:1; 
	if (!strcasecmp(msg->key,"Comment-To")) return 0;
	if (!strcasecmp(msg->key,"X-Comment-To")) return 0;
	if (!strcasecmp(msg->key,"Keywords")) return 2;
	if (!strcasecmp(msg->key,"Summary")) return 2;
	if (!strcasecmp(msg->key,"MIME-Version")) return removemime?0:1;
	if (!strcasecmp(msg->key,"Content-Type")) return removemime?0:1;
	if (!strcasecmp(msg->key,"Content-Length")) return removemime?0:1;
	if (!strcasecmp(msg->key,"Content-Transfer-Encoding")) return removemime?0:1;
	if (!strcasecmp(msg->key,"Content-Name")) return 2;
	if (!strcasecmp(msg->key,"Content-Description")) return 2;
	if (!strcasecmp(msg->key,"Message-ID")) return ftnorigin?0:1;
	if (!strcasecmp(msg->key,"References")) return removeref?0:1;
	if (!strcasecmp(msg->key,"Distribution")) return ftnorigin?0:1;
	/*if (!strcasecmp(msg->key,"")) return ;*/
	return 1;
}

/* in real life, this never happens...  I probably should drop it... */
int isftnpath(path)
char *path;
{
	char *p,*q,*r;
	faddr *fa;
	int nextftn=1,prevftn=1;

	debug(3,"checking path \"%s\" for being clear ftn",S(path));
	p=xstrcpy(path);
	if (p == NULL) return 0;
	for (q=p,r=strchr(q,'!');*q;q=r,r=strchr(q,'!'))
	{
		if (r == NULL) r=q+strlen(q);
		else *r++='\0';
		while ((*q) && (isspace(*q))) q++;
		prevftn=nextftn;
		if ((fa=parsefaddr(q)))
		{
			debug(3,"\"%s\" is ftn address",q);
			tidy_faddr(fa);
		}
		else
		{
			debug(3,"\"%s\" is not ftn address",q);
			nextftn=0;
		}
	}
	free(p);
	debug(3,"this is%s clear ftn path",prevftn?"":" not");
	return prevftn;
}

static char *months[] = {
"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"
};
static char *weekday[] = {
"Sun","Mon","Tue","Wed","Thu","Fri","Sat"
};

char *viadate(void);
char *viadate(void)
{
	static char buf[64];
	time_t t;
	struct tm *ptm;

	time(&t);
	ptm=localtime(&t);
	sprintf(buf,"%s %s %d %d at %02d:%02d",
		weekday[ptm->tm_wday],months[ptm->tm_mon],
		ptm->tm_mday,ptm->tm_year+1900,ptm->tm_hour,ptm->tm_min);
	return buf;
}

int putmessage(msg,fmsg,fp,route,flavor,sbl)
rfcmsg *msg;
ftnmsg *fmsg;
FILE *fp;
faddr *route;
char flavor;
fa_list **sbl;
{
	char /*buf[BUFSIZ],*/ *p,*q,newsubj[MAXSUBJ+1],*oldsubj;
	rfcmsg *tmp;
	int rfcheaders;
	int needsplit,hdrsize,/*datasize,*/splitpart,forbidsplit;
	int tinyorigin=0;
	faddr *ta;
	fa_list *tmpl;
	fa_list *ptl=NULL;
	int seenlen;
	int oldnet;
	int i;
	char sbe[16];
	FILE *pkt;

	debug(3,"putmessage from %s",ascfnode(fmsg->from,0x7f));
	debug(3,"putmessage   to %s",ascfnode(fmsg->to,0x7f));
	debug(3,"putmessage subj %s",S(fmsg->subj));
	debug(3,"putmessage flags %04x",fmsg->flags);
	debug(3,"putmessage msgid %s %lu",S(fmsg->msgid_a),fmsg->msgid_n);
	debug(3,"putmessage reply %s %lu",S(fmsg->reply_a),fmsg->reply_n);
	debug(3,"putmessage date %s",ftndate(fmsg->date));

	removemime=0;
	removeorg=0;
	removeref=0;
	ftnorigin=fmsg->ftnorigin;

	q=hdr("Content-Transfer-Encoding",msg);
	if (q) while (*q && isspace(*q)) q++;
	if ((p=hdr("Content-Type",msg)))
	{
		while (*p && isspace(*p)) p++;
		if ((strncasecmp(p,"text/plain",10) == 0) &&
		    ((q == NULL) ||
		     (strncasecmp(q,"7bit",4) == 0) ||
		     (strncasecmp(q,"8bit",4) == 0)))
			removemime=1; /* no need in MIME headers */
	}

	if ((p=hdr("References",msg)))
	{
		p=xstrcpy(p);
		q=strtok(p," \t\n");
		if ((q) &&
		    (strtok(NULL," \t\n") == NULL) &&
		    ((ta=parsefaddr(q))))
		{
			if (ta->name &&
			    (strspn(ta->name,"0123456789") ==
			     strlen(ta->name)))
				removeref=1;
			tidy_faddr(ta);
		}
		free(p);
	}

	p=ascfnode(fmsg->from,0x1f);
	i=79-11-3-strlen(p);
	if (ftnorigin && fmsg->origin && (strlen(fmsg->origin) > i))
	{
		/* This is a kludge...  I don't like it too much.  But well,
		   if this is a message of FTN origin, the original origin (:)
		   line MUST have been short enough to fit in 79 chars...
		   So we give it a try.  Probably it would be better to keep
		   the information about the address format from the origin
		   line in a special X-FTN-... header, but this seems even
		   less elegant.  Any _good_ ideas, anyone? */

		/* OK, I am keeping this, though if should never be used
		   al long as X-FTN-Origin is used now */

		p=ascfnode(fmsg->from,0x0f);
		i=79-11-3-strlen(p);
		tinyorigin=1;
	}
	if (fmsg->origin)
	{
		if (strlen(fmsg->origin) > i)
			fmsg->origin[i]='\0';
		else
			removeorg=1;
	}

	forbidsplit=(ftnorigin || (((p=hdr("X-FTN-Split",msg))) &&
			(strcasecmp(p," already\n") == 0)));
	needsplit=0;
	splitpart=0;
	hdrsize=20;
	hdrsize+=(fmsg->subj)?strlen(fmsg->subj):0;
	hdrsize+=(fmsg->from->name)?strlen(fmsg->from->name):0;
	hdrsize+=(fmsg->to->name)?strlen(fmsg->to->name):0;
	do
	{
		/*datasize=0;*/

		if (splitpart)
		{
			sprintf(newsubj,"[part %d] ",splitpart+1);
			strncat(newsubj,fmsg->subj,MAXSUBJ-strlen(newsubj));
		}
		else
		{
			strncpy(newsubj,fmsg->subj,MAXSUBJ);
		}
		newsubj[MAXSUBJ]='\0';

		if (splitpart) hash_update_n(&fmsg->msgid_n,splitpart);

		oldsubj=fmsg->subj;
		fmsg->subj=newsubj;
		if ((pkt=ftnmsghdr(fmsg,NULL,route,flavor)) == NULL)
		{
			return 1;
		}
		fmsg->subj=oldsubj;

		if ((p=hdr("X-FTN-REPLYADDR",msg)))
		{
			hdrsize += 10+strlen(p);
			fprintf(pkt,"\1REPLYADDR:");
			kwrite(p,pkt);
		}
		else if (replyaddr)
		{
			hdrsize += 10+strlen(replyaddr);
			fprintf(pkt,"\1REPLYADDR: ");
			kwrite(replyaddr,pkt);
		}

		if ((p=hdr("X-FTN-REPLYTO",msg)))
		{
			hdrsize += 8+strlen(p);
			fprintf(pkt,"\1REPLYTO:");
			kwrite(p,pkt);
		}
		else if (replyaddr)
		{
			hdrsize += 15;
			if (magicname)
				fprintf(pkt,"\1REPLYTO: %s %s\r",
					ascfnode(bestaka,0x1f),
					magicname);
			else
				fprintf(pkt,"\1REPLYTO: %s\r",
					ascfnode(bestaka,0x1f));
		}

		if ((p=strip_flags(hdr("X-FTN-FLAGS",msg))))
		{
			hdrsize += 15;
			fprintf(pkt,"\1FLAGS:%s\r",p);
			free(p);
		}

		if ((splitpart == 0) || (hdrsize < MAXHDRSIZE))
		{
			for (tmp=msg;tmp;tmp=tmp->next) 
			if (!strncmp(tmp->key,"X-FTN-",6) &&
			    strcasecmp(tmp->key,"X-FTN-Tearline") &&
			    strcasecmp(tmp->key,"X-FTN-Origin") &&
			    strcasecmp(tmp->key,"X-FTN-Sender") &&
			    strcasecmp(tmp->key,"X-FTN-Split") &&
			    strcasecmp(tmp->key,"X-FTN-FLAGS") &&
			    strcasecmp(tmp->key,"X-FTN-AREA") &&
			    strcasecmp(tmp->key,"X-FTN-MSGID") &&
			    strcasecmp(tmp->key,"X-FTN-REPLY") &&
			    strcasecmp(tmp->key,"X-FTN-SEEN-BY") &&
			    strcasecmp(tmp->key,"X-FTN-PATH") &&
			    strcasecmp(tmp->key,"X-FTN-REPLYADDR") &&
			    strcasecmp(tmp->key,"X-FTN-REPLYTO") &&
			    strcasecmp(tmp->key,"X-FTN-Via"))
			if (strcasecmp(tmp->key,"X-FTN-KLUDGE") == 0)
			{
				hdrsize += strlen(tmp->val);
				fprintf(pkt,"\1");
		/* we should have restored the original string here... */
				kwrite(tmp->val,pkt);
			}
			else
			{
				hdrsize += strlen(tmp->key)+strlen(tmp->val);
				fprintf(pkt,"\1%s:",tmp->key+6);
				kwrite(tmp->val,pkt);
			}

			rfcheaders=0;
			for (tmp=msg;tmp;tmp=tmp->next)
			if ((needputrfc(tmp) == 1) &&
			    ((strcasecmp(tmp->key,"Message-ID")) ||
			     (splitpart == 0)))
			{
				hdrsize += strlen(tmp->key)+strlen(tmp->val);
				fprintf(pkt,"\1RFC-%s:",tmp->key);
				kwrite(tmp->val,pkt);
			}
			for (tmp=msg;tmp;tmp=tmp->next)
			if ((needputrfc(tmp) == 2) &&
			    ((strcasecmp(tmp->key,"Message-ID")) ||
			     (splitpart == 0)))
			{
				rfcheaders++;
				hdrsize += strlen(tmp->key)+strlen(tmp->val);
				fprintf(pkt,"%s:",tmp->key);
				cwrite(tmp->val,pkt);
			}
			if (rfcheaders) cwrite("\n",pkt);
		}

		if (replyaddr)
		{
			free(replyaddr);
			replyaddr=NULL;
		}
/*
		if (needsplit)
		{
			fprintf(pkt," * Continuation %d of a split message *\r\r",
				splitpart);
			needsplit=0;
		}
		else if ((p=hdr("X-Body-Start",msg))) 
		{
			datasize += strlen(p);
			cwrite(p,pkt);
		}
		while (!(needsplit=(!forbidsplit) &&
				(((splitpart &&
				   (datasize > maxmsize)) ||
				  (!splitpart &&
				   ((datasize+hdrsize) > maxmsize))))
							) &&
			(bgets(buf,sizeof(buf)-1,fp)))
		{
			debug(19,"putmessage body %s",buf);
			datasize += strlen(buf);
			cwrite(buf,pkt);
		}
		if (needsplit)
		{
			fprintf(pkt,"\r * Message split, to be continued *\r");
			splitpart++;
		}
*/			

		putbody(0,msg,fp,pkt,forbidsplit,hdrsize);

		if ((p=hdr("X-FTN-Tearline",msg)))
		{
			fprintf(pkt,"---");
			if (strcasecmp(p," (none)\n") == 0)
				cwrite("\n",pkt);
			else
				cwrite(p,pkt);
		}
		else if (newsmode) fprintf(pkt,"--- ifmail v.%s\r",version);
		if ((p=hdr("X-FTN-Origin",msg)))
		{
			if (*(q=p+strlen(p)-1) == '\n') *q='\0';
			fprintf(pkt," * Origin:");
			cwrite(p,pkt);
			if (!newsmode) fprintf(pkt,"\r");
		}
		else if (newsmode)
		{
			fprintf(pkt," * Origin: "); /* strlen=11 */
			if (fmsg->origin)
				cwrite(fmsg->origin,pkt);
			else
				cwrite("Unknown",pkt);
			fprintf(pkt," (%s)",
				ascfnode(fmsg->from,tinyorigin?0x0f:0x1f));
		}

		if (newsmode)
		{
			for (tmpl=whoami;tmpl;tmpl=tmpl->next)
			if ((tmpl->addr->point == 0) &&
			    ((bestaka->domain == NULL) ||
			     (tmpl->addr->domain == NULL) ||
			     (strcasecmp(bestaka->domain,
					tmpl->addr->domain) == 0)) &&
			    (bestaka->zone == tmpl->addr->zone))
				fill_list(sbl,ascfnode(tmpl->addr,0x06),NULL);
#ifdef HAS_NDBM_H
			if ((p=hdr("Message-ID",msg)))
			{
				while (isspace(*p)) p++;
				q=xstrcpy(p);
				if (*(p=q+strlen(q)-1) == '\n') *(p--)='\0';
				while (isspace(*p)) *(p--)='\0';
				fill_list(sbl,idlookup(q),NULL);
				free(q);
			}
#endif
			sort_list(sbl);
			seenlen=MAXSEEN+1;
			/* ensure it will not match for the first entry */
			oldnet=(*sbl)->addr->net-1;
			for (tmpl=*sbl;tmpl;tmpl=tmpl->next)
			{
				if (tmpl->addr->net == oldnet)
					sprintf(sbe," %u",tmpl->addr->node);
				else
					sprintf(sbe," %u/%u",tmpl->addr->net,
							tmpl->addr->node);
				oldnet=tmpl->addr->net;
				seenlen+=strlen(sbe);
				if (seenlen > MAXSEEN)
				{
					seenlen=0;
					fprintf(pkt,"\rSEEN-BY:");
					sprintf(sbe," %u/%u",tmpl->addr->net,
							tmpl->addr->node);
					seenlen=strlen(sbe);
				}
				fprintf(pkt,"%s",sbe);
			}

			for (tmp=msg;tmp;tmp=tmp->next) 
			if (!strcasecmp(tmp->key,"X-FTN-PATH"))
			{
				fill_path(&ptl,tmp->val);
			}
			sprintf(sbe,"%u/%u",bestaka->net,
				bestaka->node);
			fill_path(&ptl,sbe);
			uniq_list(&ptl);
			seenlen=MAXPATH+1;
			/* ensure it will not match for the first entry */
			oldnet=ptl->addr->net-1;
			for (tmpl=ptl;tmpl;tmpl=tmpl->next)
			{
				if (tmpl->addr->net == oldnet)
					sprintf(sbe," %u",tmpl->addr->node);
				else
					sprintf(sbe," %u/%u",tmpl->addr->net,
							tmpl->addr->node);
				oldnet=tmpl->addr->net;
				seenlen+=strlen(sbe);
				if (seenlen > MAXPATH)
				{
					seenlen=0;
					fprintf(pkt,"\r\1PATH:");
					sprintf(sbe," %u/%u",tmpl->addr->net,
							tmpl->addr->node);
					seenlen=strlen(sbe);
				}
				fprintf(pkt,"%s",sbe);
			}
			fprintf(pkt,"\r");
		}
		else /* mail mode */
		{
			for (tmp=msg;tmp;tmp=tmp->next) 
			if (!strcasecmp(tmp->key,"X-FTN-Via"))
			{
			/*datasize += strlen(tmp->key)+strlen(tmp->val);*/
				fprintf(pkt,"\1Via");
				kwrite(tmp->val,pkt);
			}
			fprintf(pkt,"\1Via ifmail %s, %s (%s)\r",
				ascfnode(bestaka,0x1f),
				viadate(),version);
		}
		awrite("",pkt); /* trailing zero byte */
		if (ferror(pkt))
		{
			logerr("$error writing to ftn packet");
			return 1;
		}
		tidy_falist(&ptl);
	}
	while (needsplit);

	debug(3,"putmessage exiting...");
	return 0;
}

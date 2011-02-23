#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <ctype.h>
#include "lutil.h"
#include "xutil.h"
#include "mkrfcmsg.h"
#include "ftnmsg.h"
#include "rfcmsg.h"
#include "areas.h"
#include "falists.h"
#include "config.h"

#define BOUNDARY 79

int num_echo=0,num_mail=0;
extern char *version;
extern FILE *nb;
extern faddr pktfrom;
extern int notransports;

extern char *rfcdate(time_t);
extern unsigned INT32 sequencer(void);
extern unsigned INT16 crc(char*);
extern char *lookup(char *);
extern char *backalias(faddr *);

extern FILE *expipe(char*,char*,char*);
extern int exclose(FILE*);
extern char *compose_flags(int,char *);

char *rfcmsgid(msgid,bestaka)
char *msgid;
faddr *bestaka;
{
	static char buf[128],*p,*q,*r;
	unsigned long id=0L;
	faddr *ta=NULL;

	if ((p=strrchr(msgid,' ')))
	{
		*p='\0';
		sscanf(p+1,"%lx",&id);
		ta=parsefnode(msgid);
		*p=' ';
	}
	if (id != 0L)
	{
		if (ta)
		{
			sprintf(buf,"<%lu@%s.ftn>",id,ascinode(ta,0x1f));
		}
		else
		{
			p=xstrcpy(msgid);
			if ((q=strchr(p,' '))) *q='\0';
			q=p+strlen(p)-1;
			if ((*p == '<') && (*q == '>'))
			{
				*q='\0';
				q=p+1;
			}
			else q=p;
			sprintf(buf,"<%lu@%s>",id,q);
			if ((r=strrchr(buf,'@'))) {
				for (q=buf;q<r;q++)
					if (*q == '@') (*q)='%';
			}
			free(p);
		}
	}
	else
	{
		sprintf(buf,"<%lu@%s.ftn>",(unsigned long)sequencer(),
			ascinode(bestaka,0x1f));
	}
	tidy_faddr(ta);
	return buf;
}

/* check address for localness, substitute alises and replace it *in place* */

void substitute(buf)
char *buf;
{
	faddr *fa;
	char *l,*r,*p=NULL;
	int inquotes,inbrackets;

	debug(6,"to address before subst: \"%s\"",buf);
	if ((l=strchr(buf,'<')) && (r=strchr(buf,'>')) && (l < r))
	{
		l++;
		*r='\0';
	}
	else l=buf;
	while (*l == ' ') l++;
	for (r=l,inquotes=0,inbrackets=0;*r;r++) switch (*r)
	{
	case '"':	inquotes=(!inquotes); break;
	case ',':
	case ' ':	if (!inquotes && !inbrackets) *r='\0'; break;
	case '(':	if (!inquotes) inbrackets++; break;
	case ')':	if (!inquotes && inbrackets) inbrackets--; break;
	default:	break;
	}
	if ((fa=parsefaddr(l)))
	{
		debug(6,"it is an ftn address: %s",ascfnode(fa,0x7f));
		if (is_local(fa))
		{
			debug(6,"it is local");
			sprintf(buf,"%s",fa->name);
			if (!strchr(buf,'@') && (p=strrchr(buf,'%')))
				*p='@';
			if (!strchr(buf,'@'))
			{
				if ((p=lookup(buf)))
					strcpy(buf,p);
				else for (p=buf;*p;p++)
					if (*p == ' ') *p='.';
			}
			if (!strcasecmp(buf,"sysop")) strcpy(buf,"postmaster");
		}
		else
		{
			debug(6,"it is not local");
			sprintf(buf,"%s",ascinode(fa,0x7f));
		}
		tidy_faddr(fa);
	}
	else 
	{
		debug(6,"it is not ftn address");
		for (r=buf;*l;l++,r++) *r=*l;
		*r='\0';
	}
	if (buf[0] == '\0') strcpy(buf,"postmaster");
	debug(6,"to address after  subst: \"%s\"",buf);
	return;
}

/* from, to, subj, orig, mdate, flags, file, offset, kluges */

int mkrfcmsg(f,t,subj,orig,mdate,flags,fp,endoff,kmsg)
faddr *f,*t;
char *subj,*orig;
time_t mdate;
int flags;
FILE *fp;
off_t endoff;
rfcmsg *kmsg;
{
	char buf[BUFSIZ];
	char *p,*q,*l,*r,*b;
	char *newsgroup=NULL,*distribution=NULL;
	char c;
	int count,rrq;
	rfcmsg *msg=NULL,*tmsg;
	FILE *pip;
	struct stat stbuf;
	int newsmode,pass,rc;
	faddr *ta,*bestaka;
	fa_list *ftnpath=NULL,*tfa,*rlist;
	time_t now;
	int lines;
	char lineshdr[16];
	moderator_list *tmpml;

	if ((magicname == NULL) ||
	    ((t->name) && (strcasecmp(magicname,t->name) == 0)))
		msg=parsrfc(fp);
	bestaka=bestaka_s(f);

	if (kmsg && !strcmp(kmsg->key,"AREA"))
	{
		ngdist(kmsg->val,&newsgroup,&distribution);
		if (!newsgroup)
		{
			tidyrfc(msg);
			return 0;
		}
		newsmode=1;
	}
	else newsmode=0;
	debug(5,"Got %s message",newsmode?"echo":"netmail");

	if ((p=hdr("DOMAIN",kmsg)))
	{
		strncpy(buf,p,sizeof(buf)-1);
		buf[sizeof(buf)-1]='\0';
		l=strtok(buf," \n");
		p=strtok(NULL," \n");
		r=strtok(NULL," \n");
		q=strtok(NULL," \n");
		if ((ta=parsefnode(p)))
		{
			t->point=ta->point;
			t->node=ta->node;
			t->net=ta->net;
			t->zone=ta->zone;
			tidy_faddr(ta);
		}
		t->domain=xstrcpy(l);
		if ((ta=parsefnode(q)))
		{
			f->point=ta->point;
			f->node=ta->node;
			f->net=ta->net;
			f->zone=ta->zone;
			tidy_faddr(ta);
		}
		f->domain=xstrcpy(r);
	}
	else if ((p=hdr("INTL",kmsg)))
	{
		strncpy(buf,p,sizeof(buf)-1);
		buf[sizeof(buf)-1]='\0';
		l=strtok(buf," \n");
		r=strtok(NULL," \n");
		if ((ta=parsefnode(l)))
		{
			t->point=ta->point;
			t->node=ta->node;
			t->net=ta->net;
			t->zone=ta->zone;
			if (ta->domain)
			{
				if (t->domain) free(t->domain);
				t->domain=ta->domain;
				ta->domain=NULL;
			}
			tidy_faddr(ta);
		}
		if ((ta=parsefnode(r)))
		{
			f->point=ta->point;
			f->node=ta->node;
			f->net=ta->net;
			f->zone=ta->zone;
			if (ta->domain)
			{
				if (f->domain) free(f->domain);
				f->domain=ta->domain;
				ta->domain=NULL;
			}
			tidy_faddr(ta);
		}
	}

	if ((p=hdr("FMPT",kmsg))) f->point=atoi(p);
	if ((p=hdr("TOPT",kmsg))) t->point=atoi(p);

	debug(5,"final from: %s",ascfnode(f,0x1f));
	debug(5,"final   to: %s",ascfnode(t,0x1f));

	if (!newsmode)
	{
		p=hdr("Resent-To",msg);
		if (p == NULL) p=hdr("To",msg);
		if (p == NULL) p=hdr("RFC-Resent-To",kmsg);
		if (p == NULL) p=hdr("RFC-To",kmsg);
		if (p && is_local(t))
		{
			while (*p == ' ') p++;
			strncpy(buf,p,sizeof(buf)-1);
			if (*(p=buf+strlen(buf)-1) == '\n') *p='\0';
		}
		else sprintf(buf,"%s",ascinode(t,0x7f));
		substitute(buf);
		loginf("mail from %s to %s",ascfnode(f,0x7f),buf);
	}

	if ((newsmode) && (nb == NULL))
	{
		if (notransports)
		{
			nb=fopen(tempnam("/tmp/ifmail","N."),"w");
			fprintf(nb,"#!/bin/sh\n%s <<__EOF__\n",S(rnews));
		}
		else nb=expipe(rnews,NULL,NULL);
		if (nb == NULL)
		{
			logerr("$Could non open (pipe) output for news");
			tidyrfc(msg);
			return 2;
		}
	}

	if (newsmode)
	{
		if ((pip=tmpfile()) == NULL)
		{
			logerr("$Cannot open second temporary file");
			tidyrfc(msg);
			return 3;
		}
	}
	else
	{
		debug(2,"expipe(\"%s\",\"%s\",\"%s\")",
			S(sendmail),ascinode(f,0x7f),buf);
		if (notransports)
		{
			pip=fopen(tempnam("/tmp/ifmail","M."),"w");
			fprintf(pip,"#!/bin/sh\nF=%s\nT=%s\n%s <<__EOF__\n",
				ascinode(f,0x7f),buf,S(sendmail));
		}
		else pip=expipe(sendmail,ascinode(f,0x7f),buf);
		if (pip == NULL)
		{
			logerr("$Could non open (pipe) output for mail");
			tidyrfc(msg);
			return 2;
		}
	}


	if (newsmode)
	{
		num_echo++;
		if ((p=hdr("Path",msg)) == NULL)
			p=hdr("RFC-Path",kmsg);
		rlist=NULL;
		fill_rlist(&rlist,p);
		for (tmsg=kmsg;tmsg;tmsg=tmsg->next)
			if (strcasecmp(tmsg->key,"PATH") == 0)
				fill_list(&ftnpath,tmsg->val,&rlist);
		tidy_falist(&rlist);

		fprintf(pip,"Path: ");
		
		fprintf(pip,"%s!",ascinode(bestaka,0x07));
		fprintf(pip,"%s!",ascinode(&pktfrom,0x07));
		if (ftnpath)
			for (tfa=ftnpath->next;tfa;tfa=tfa->next)
				fprintf(pip,"%s!",ascinode(tfa->addr,0x06));
		tidy_falist(&ftnpath);

		if (p)
		{
			while (isspace(*p)) p++;
			fprintf(pip,"%s",p);
		}
		else fprintf(pip,"not-for-mail\n");

		fprintf(pip,"Newsgroups: %s\n",newsgroup);
		if (distribution)
			fprintf(pip,"Distribution: %s\n",distribution);
		if (t->name == NULL) t->name=xstrcpy("All");
		p=hdr("Comment-To",msg);
		if (p == NULL) p=hdr("X-Comment-To",msg);
		if (p == NULL) p=hdr("To",msg);
		if (p)
			fprintf(pip,"X-Comment-To:%s",p);
		else
		{
			if (p == NULL) p=hdr("RFC-X-Comment-To",kmsg);
			if (p == NULL) p=hdr("RFC-Comment-To",kmsg);
			if (p == NULL) p=hdr("RFC-To",kmsg);
			if (p) fprintf(pip,"X-Comment-To: %s",p);
			else if (t->name)
				fprintf(pip,"X-Comment-To: %s\n",t->name);
		}
		for (tmpml=approve;tmpml;tmpml=tmpml->next) {
			if ((strncmp(newsgroup,tmpml->prefix,
					strlen(tmpml->prefix)) == 0) &&
			    (hdr("RFC-Approved",kmsg) == NULL)) {
				fprintf(pip,"Approved: %s\n",tmpml->address);
				break;
			}
		}
	}
	else
	{
		num_mail++;
		time(&now);
#ifdef NEED_UUCPFROM
		fprintf(pip,"From %s!",ascinode(f,0x3f));
		fprintf(pip,"%s %s",ascinode(f,0x40),ctime(&mdate));
#endif
		fprintf(pip,"Received: from %s ",ascinode(f,0x3f));
		fprintf(pip,"by %s\n",ascinode(bestaka,0x3f));
		fprintf(pip,"\twith FTN (ifmail v.%s) id AA%u; %s\n",
			version,getpid(),rfcdate(now));
		for (tmsg=kmsg;tmsg;tmsg=tmsg->next)
			if (!strcasecmp(tmsg->key,"RFC-Received"))
				fprintf(pip,"%s:%s",tmsg->key+4,tmsg->val);
		for (tmsg=msg;tmsg;tmsg=tmsg->next)
			if (!strcasecmp(tmsg->key,"Received"))
				fprintf(pip,"%s:%s",tmsg->key,tmsg->val);
		if ((hdr("Apparently-To",msg) == NULL) && is_local(t))
			fprintf(pip,"Apparently-To: %s\n",buf);
		if (t->name == NULL) t->name=xstrcpy("Sysop");
		p=hdr("Resent-To",msg);
		if (p == NULL) p=hdr("To",msg);
		if (p)
			fprintf(pip,"To:%s",p);
		else
		{
			if (p == NULL) p=hdr("RFC-Resent-To",kmsg);
			if (p == NULL) p=hdr("RFC-To",kmsg);
			if (p) fprintf(pip,"To: %s",p);
			else fprintf(pip,"To: %s\n",ascinode(t,0xff));
		}
	}

	if ((p=hdr("From",msg))) fprintf(pip,"From:%s",p);
	else if ((p=hdr("RFC-From",kmsg))) fprintf(pip,"From: %s",p);
	else if ((p=xstrcpy(hdr("REPLYADDR",kmsg))))
	{
		if (*(r=p+strlen(p)-1) == '\n') *(r--)='\0';
		while (isspace(*r)) *(r--)='\0';
		q=xstrcpy(hdr("X-RealName",msg));
		if (q == NULL) q=xstrcpy(hdr("RealName",msg));
		if (q == NULL) q=xstrcpy(hdr("X-RealName",kmsg));
		if (q == NULL) q=xstrcpy(hdr("RealName",kmsg));
		if (q)
		{
			if (*(r=q+strlen(q)-1) == '\n') *(r--)='\0';
			while (isspace(*r)) *(r--)='\0';
			for (l=q;isspace(*l);) l++;
			if ((*l == '\"') && (*r == '\"'))
			{
				l++;
				*r--='\0';
			}
			fprintf(pip,"From: \"%s\" <%s>\n",l,p);
			free(q);
		}
		else
		{
			fprintf(pip,"From: %s\n",p);
		}
		free(p);
	}

	if (p) fprintf(pip,"X-FTN-Sender: %s\n",ascinode(f,0xff));
	else fprintf(pip,"From: %s\n",ascinode(f,0xff));

	if ((p=hdr("Reply-To",msg))) fprintf(pip,"Reply-To:%s",p);
	else if ((p=hdr("RFC-Reply-To",kmsg))) fprintf(pip,"Reply-To: %s",p);
	else if (((p=backalias(f))) && myfqdn)
		fprintf(pip,"Reply-To: %s@%s\n",p,myfqdn);
	else if ((p=hdr("REPLYADDR",kmsg))) fprintf(pip,"Reply-To: %s",p);

	if ((p=hdr("Date",msg))) fprintf(pip,"Date:%s",p);
	else if ((p=hdr("RFC-Date",kmsg))) fprintf(pip,"Date: %s",p);
	else fprintf(pip,"Date: %s\n",rfcdate(mdate));

	if ((p=hdr("Subject",msg))) fprintf(pip,"Subject:%s",p);
	else if ((p=hdr("RFC-Subject",kmsg))) fprintf(pip,"Subject: %s",p);
	else if (subj && (strspn(subj," \t\n\r") != strlen(subj)))
		fprintf(pip,"Subject: %s\n",subj);
	else fprintf(pip,"Subject: <none>\n");

	if ((p=hdr("Message-ID",msg)))
		fprintf(pip,"Message-ID:%s",p);
	else if ((p=hdr("RFC-Message-ID",kmsg)))
		fprintf(pip,"Message-ID: %s",p);
	else if ((p=hdr("Message-ID",kmsg)))
		fprintf(pip,"Message-ID: %s",p);
	else if ((p=hdr("RFCID",kmsg)))
		fprintf(pip,"Message-ID: <%s>",p);
	else if ((p=hdr("MSGID",kmsg)))
		fprintf(pip,"Message-ID: %s\n",rfcmsgid(p,bestaka));
	else fprintf(pip,"Message-ID: <%lu@%s.ftn>\n",
			mdate^(subj?crc(subj):0L),
			ascinode(f,0x1f));

	if (newsmode)
	{
		if ((p=hdr("References",msg)))
			fprintf(pip,"References:%s",p);
		else if ((p=hdr("RFC-References",kmsg)))
			fprintf(pip,"References: %s",p);
		else if ((p=hdr("REPLY",kmsg)))
			fprintf(pip,"References: %s\n",rfcmsgid(p,bestaka));
	}
	else
	{
		if ((p=hdr("In-Reply-To",msg)))
			fprintf(pip,"In-Reply-To:%s",p);
		else if ((p=hdr("RFC-In-Reply-To",kmsg)))
			fprintf(pip,"In-Reply-To: %s",p);
		else 
		{
			if ((p=hdr("REPLY",kmsg)))
				fprintf(pip,"In-Reply-To: %s\n",
					rfcmsgid(p,bestaka));
		}
	}

	if ((p=hdr("Organization",msg))) fprintf(pip,"Organization:%s",p);
	else if ((p=hdr("RFC-Organization",kmsg)))
		fprintf(pip,"Organization: %s",p);
	else if ((p=hdr("Organization",kmsg)))
		fprintf(pip,"Organization: %s",p);
	else if (orig) fprintf(pip,"Organization: %s\n",orig);

	for (tmsg=msg;tmsg;tmsg=tmsg->next)
	{
		if (strcasecmp(tmsg->key,"X-Body-Start") &&
		    strcasecmp(tmsg->key,"Lines") &&
		    strcasecmp(tmsg->key,"Path") &&
		    strcasecmp(tmsg->key,"Received") &&
		    strcasecmp(tmsg->key,"From") &&
		    strcasecmp(tmsg->key,"To") &&
		    strcasecmp(tmsg->key,"Comment-To") &&
		    strcasecmp(tmsg->key,"X-Comment-To") &&
		    strcasecmp(tmsg->key,"Date") &&
		    strcasecmp(tmsg->key,"Subject") &&
		    strcasecmp(tmsg->key,"Reply-To") &&
		    strcasecmp(tmsg->key,"In-Reply-To") &&
		    strcasecmp(tmsg->key,"References") &&
		    strcasecmp(tmsg->key,"Organization") &&
		    (strcasecmp(tmsg->key,"Newsgroups") || !newsmode) &&
		    strcasecmp(tmsg->key,"Distribution") &&
		    strcasecmp(tmsg->key,"Message-ID"))
			fprintf(pip,"%s:%s",tmsg->key,tmsg->val);
	}

	if ((p=compose_flags(flags,hdr("FLAGS",kmsg))))
	{
		fprintf(pip,"X-FTN-FLAGS:%s\n",p);
		free(p);
	}

	if (flags & FLG_RRQ) rrq=1;
	else rrq=0;
	if (rrq && !hdr("RFC-Return-Receipt-To",kmsg) && 
	    !hdr("Return-Receipt-To",msg))
	{
		if ((p=hdr("REPLYADDR",kmsg)))
			fprintf(pip,"Return-Receipt-To: %s",p);
		else if (((p=backalias(f))) && myfqdn)
			fprintf(pip,"Reply-To: %s@%s\n",p,myfqdn);
		else fprintf(pip,"Return-Receipt-To: %s\n",ascinode(f,0x7f));
	}

	for (tmsg=kmsg;tmsg;tmsg=tmsg->next)
	{
		if (strcasecmp(tmsg->key,"INTL") &&
		    strcasecmp(tmsg->key,"FMPT") &&
		    strcasecmp(tmsg->key,"TOPT") &&
		    strcasecmp(tmsg->key,"FLAGS") &&
		    strcasecmp(tmsg->key,"RFC-Lines") &&
		    strcasecmp(tmsg->key,"RFC-Path") &&
		    strcasecmp(tmsg->key,"RFC-Received") &&
		    strcasecmp(tmsg->key,"RFC-From") &&
		    strcasecmp(tmsg->key,"RFC-To") &&
		    strcasecmp(tmsg->key,"RFC-Comment-To") &&
		    strcasecmp(tmsg->key,"RFC-X-Comment-To") &&
		    strcasecmp(tmsg->key,"RFC-Date") &&
		    strcasecmp(tmsg->key,"RFC-Subject") &&
		    strcasecmp(tmsg->key,"RFC-Reply-To") &&
		    strcasecmp(tmsg->key,"RFC-In-Reply-To") &&
		    strcasecmp(tmsg->key,"RFC-References") &&
		    strcasecmp(tmsg->key,"RFC-Organization") &&
		    strcasecmp(tmsg->key,"RFC-Newsgroups") &&
		    strcasecmp(tmsg->key,"RFC-Distribution") &&
		    strcasecmp(tmsg->key,"RFC-Message-ID"))
			if (!strncmp(tmsg->key,"RFC-",4))
				fprintf(pip,"%s: %s",tmsg->key+4,tmsg->val);
			else
				fprintf(pip,"X-FTN-%s: %s",tmsg->key,tmsg->val);
	}

	if (newsmode)
	{
		fprintf(pip,"X-FTN-PATH: %s\n",ascfnode(bestaka,0x06));
	}

	fprintf(pip,"\n");
	lines=0;
	if ((p=hdr("X-Body-Start",msg)))
	{
		lines++;
		fputs(p,pip);
	}
	pass=1;
	count=0;
	while(fgets(buf,sizeof(buf)-1,fp) && pass)
	{
		if (ftell(fp) > endoff)
		{
			debug(5,"line \"%s\" past message end %ld",
				buf,(long)endoff);
			pass=0;
		}
		if (pass)
		{
			p=buf;
			b=NULL;
			while ((c=*p++))
			{
				switch (c)
				{
				case ' ':	b=p-1; break;
				case '\n':	b=NULL; count=0; lines++; break;
				}
				if (count++ > BOUNDARY)
				{
					if (b) 
					{
						*b='\n';
						p=b+1;
						b=NULL;
						lines++;
						count=0;
					}
				}
			}
			fputs(buf,pip);
		}
	}

	if (newsmode)
	{
		sprintf(lineshdr,"Lines: %d\n",lines);
		rewind(pip);
		fstat(fileno(pip),&stbuf);
		fprintf(nb,"#! rnews %lu\n",
			(unsigned long)stbuf.st_size+strlen(lineshdr));
		while (fgets(buf,sizeof(buf)-1,pip) && (buf[0] != '\n'))
			fputs(buf,nb);
		fputs(lineshdr,nb);
		fputs(buf,nb);
		while (fgets(buf,sizeof(buf)-1,pip))
			fputs(buf,nb);
	}
	if ((
	     (notransports || newsmode)?(rc=fclose(pip)):(rc=exclose(pip))
	   ))
	{
		logerr("$close of transport pipe or tmp file returned %d",rc);
	}
	tidyrfc(msg);
	if (rc < 0) rc=-rc;
	return rc;
}

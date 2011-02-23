#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "lutil.h"
#include "xutil.h"
#include "ftn.h"
#include "rfcaddr.h"
#include "config.h"

#ifndef MAXUSHORT
#define MAXUSHORT 65535
#endif

void tidy_faddr(addr)
faddr *addr;
{
	if (addr == NULL) return;
	if (addr->name) free(addr->name);
	if (addr->domain) free(addr->domain);
	free(addr);
}

void tidy_falist(fap)
fa_list **fap;
{
	fa_list *tmp,*old;

	debug(8,"tidy_falist beginning from %s",
		*fap?ascinode((*fap)->addr,0x06):"<null>");
	for (tmp=*fap;tmp;tmp=old)
	{
		old=tmp->next;
		tidy_faddr(tmp->addr);
		free(tmp);
	}
	*fap=NULL;
}

faddr *parsefaddr(s)
char *s;
{
	faddr *tmpaddr=NULL;
	parsedaddr rfcaddr;
	int gotzone=0,gotnet=0,gotnode=0,gotpoint=0;
	int zone=0,net=0,node=0,point=0;
	char *domain=NULL,*freename=NULL;
	long num;
	char *p,*q,*t;
	int l;
	dom_trans *dt;

	rfcaddr.target=NULL;
	rfcaddr.remainder=NULL;
	rfcaddr.comment=NULL;

	t=xstrcpy(s);
	if (*(q=t+strlen(t)-1) == '\n') *q='\0';
	if (((*(p=t) == '(') && (*(q=p+strlen(p)-1) == ')')) ||
	    ((*p == '\"') && (*q == '\"')))
	{
		p++;
		*q='\0';
	}
	if (strchr(s,'@') || strchr(s,'%') || strchr(s,'!'))
		rfcaddr=parserfcaddr(p);
	else
	{
		addrerror=0;
		rfcaddr.target=xstrcpy(p);
	}
	free(t);
	if ((addrerror) || (rfcaddr.target == NULL))
		goto leave;

	p=rfcaddr.target;
	for (dt=domtrans;dt;dt=dt->next)
	{
		q=p+strlen(p)-strlen(dt->intdom);
		if ((q >= p) && (strcasecmp(dt->intdom,q) == 0))
		{
			*q='\0';
			q=xmalloc(strlen(p)+strlen(dt->ftndom)+1);
			strcpy(q,p);
			strcat(q,dt->ftndom);
			p=q;
			free(rfcaddr.target);
			rfcaddr.target=p;
			break;
		}
	}

	if (((l=strlen(rfcaddr.target)) > 4) &&
	    (strcasecmp(rfcaddr.target+l-4,".ftn") == 0))
	{
		rfcaddr.target[l-4]='\0';
	}

/*
	for (p=strtok(rfcaddr.target,".");
		p;
		p=strtok(NULL,"."))
*/
	for (p=rfcaddr.target;
		p;
		p=q)
	{
		q=strchr(p,'.');
		if (q) *q++=0;
		if (((l=strlen(p+1)) > 0) && (l <= 5) &&
		    (strspn(p+1,"0123456789") == l))
		{
			num=atol(p+1);
			switch (*p)
			{
			case 'z':
			case 'Z':
				gotzone++;
				if (num > MAXUSHORT) addrerror |= ADDR_BADTOKEN;
				zone=num;
				break;
			case 'n':
			case 'N':
				gotnet++;
				if (num > MAXUSHORT) addrerror |= ADDR_BADTOKEN;
				net=num;
				break;
			case 'f':
			case 'F':
				gotnode++;
				if (num > MAXUSHORT) addrerror |= ADDR_BADTOKEN;
				node=num;
				break;
			case 'p':
			case 'P':
				gotpoint++;
				if (num > MAXUSHORT) addrerror |= ADDR_BADTOKEN;
				point=num;
				break;
			default:
				if (gotnet && gotnode)
				{
					if (domain == NULL)
						domain=xstrcpy(p);
				}
				else addrerror |= ADDR_BADTOKEN;
				break;
			}
		}
		else /* not "cNNNN" token */
		{
			if (gotnet && gotnode)
			{
				if (domain == NULL) domain=xstrcpy(p);
			}
			else addrerror |= ADDR_BADTOKEN;
		}
	}

	if ((gotzone > 1) || (gotnet != 1) || (gotnode != 1) || (gotpoint > 1))
	{
		addrerror |= ADDR_BADSTRUCT;
	}

	if (addrerror) goto leave;

	if (rfcaddr.remainder)
	{
		if ((*(p=rfcaddr.remainder) == '\"') &&
		    (*(q=p+strlen(p)-1) == '\"'))
		{
			p++;
			*q='\0';
		}
		if (strchr(p,'@') || strchr(p,'%') || strchr(p,'!'))
		{
			if (((q=strrchr(p,'%'))) && !strchr(p,'@'))
				*q='@';
		}
		else if (!strchr(p,' '))
		{
			for (q=p;*q;q++) if (*q == '.') *q=' ';
		}
		if (strlen(p) > MAXNAME) p[MAXNAME]='\0';
		if (strspn(p," ") != strlen(p)) freename=xstrcpy(p);
	}

	tmpaddr=(faddr*)xmalloc(sizeof(faddr));

	tmpaddr->zone=zone;
	tmpaddr->net=net;
	tmpaddr->node=node;
	tmpaddr->point=point;
	tmpaddr->domain=domain;
	domain=NULL;
	tmpaddr->name=freename;
	freename=NULL;

leave:
	if (domain) free(domain);
	if (freename) free(freename);
	tidyrfcaddr(rfcaddr);
	return tmpaddr;
}

faddr *parsefnode(s)
char *s;
{
	faddr addr,*tmp;
	char *buf,*str,*p,*q,*n;
	int good = 1;

	if (s == NULL) return NULL;

	str=buf=xstrcpy(s);

	while (isspace(*str)) str++;
	if (*(p=str+strlen(str)-1) == '\n') *(p--)='\0';
	while (isspace(*p)) *(p--) = '\0';

	p=str + strlen(str) - 1;
	if (((*str == '(') && (*p == ')')) ||
	    ((*str == '\"') && (*p == '\"')) ||
	    ((*str == '\'') && (*p == '\'')) ||
	    ((*str == '<') && (*p == '>')) ||
	    ((*str == '[') && (*p == ']')) ||
	    ((*str == '{') && (*p == '}')))
	{
		str++;
		*p='\0';
	}

	addr.name=NULL;
	if ((p=strrchr(str,' ')))
	{
		n=str;
		str=p+1;
		while (isspace(*p)) *(p--) = '\0';
		if (!strcasecmp(p-2," of")) *(p-2) = '\0';
		p-=3;
		while (isspace(*p) || (*p == ',')) *(p--) = '\0';
		if (strlen(n) > MAXNAME) n[MAXNAME]='\0';
		if (*n != '\0') addr.name=xstrcpy(n);
	}

	if ((p=strrchr(str,'@'))) *(p++)='\0';
	else if ((p=strrchr(str,'%'))) *(p++)='\0';
	else p=NULL;
	if (p && ((q=strchr(p,'.')))) *q='\0';

	addr.point=0;
	addr.node=0;
	addr.net=0;
	addr.zone=0;
	addr.domain=xstrcpy(p);

	if ((p=strchr(str,':')))
	{
		*(p++)='\0';
		if (strspn(str,"0123456789") == strlen(str))
			addr.zone=atoi(str);
		else if (strcmp(str,"*") == 0)
			addr.zone=-1;
		else good=0;
		str=p;
	}
	if ((p=strchr(str,'/')))
	{
		*(p++)='\0';
		if (strspn(str,"0123456789") == strlen(str))
			addr.net=atoi(str);
		else if (strcmp(str,"*") == 0)
			addr.net=-1;
		else good=0;
		str=p;
	}
	if ((p=strchr(str,'.')))
	{
		*(p++)='\0';
		if (strspn(str,"0123456789") == strlen(str))
			addr.node=atoi(str);
		else if (strcmp(str,"*") == 0)
			addr.node=-1;
		else good=0;
		str=p;
	}
	else
	{
		if (strspn(str,"0123456789") == strlen(str))
			addr.node=atoi(str);
		else if (strcmp(str,"*") == 0)
			addr.node=-1;
		else good=0;
		str=NULL;
	}
	if (str)
	{
		if (strspn(str,"0123456789") == strlen(str))
			addr.point=atoi(str);
		else if (strcmp(str,"*") == 0)
			addr.point=-1;
		else good=0;
	}

	if (buf) free(buf);

	if (good)
	{
		tmp=(faddr *)xmalloc(sizeof(addr));
		tmp->name=NULL;
		tmp->domain=addr.domain;
		tmp->point=addr.point;
		tmp->node=addr.node;
		tmp->net=addr.net;
		tmp->zone=addr.zone;
		return tmp;
	}
	else
	{
		if (addr.name) free(addr.name);
		if (addr.domain) free(addr.domain);
		return NULL;
	}
}

char *ascinode(a,fl)
faddr *a;
int fl;
{
	static char buf[128],*f,*t,*q;
	dom_trans *dt;
	fa_list *tmpl;
	int skip;

	if (a == NULL)
	{
		strcpy(buf,"<none>");
		return buf;
	}

	buf[0]='\0';
	if ((fl & 0x80) && (a->name))
	{
		if ((strchr(a->name,'.')) || (strchr(a->name,'@')) ||
		    (strchr(a->name,'\'')) || (strchr(a->name,',')) ||
		    (strchr(a->name,'<')) || (strchr(a->name,'>')))
			sprintf(buf+strlen(buf),"\"%s\" <",a->name);
		else
			sprintf(buf+strlen(buf),"%s <",a->name);
	}
	if ((fl & 0x40) && (a->name))
	{
		f=a->name;
		t=buf+strlen(buf);
		skip=0;
		while (*f)
		{
			switch (*f)
			{
			case '.':
			case ' ':	if (!skip) *t++='.'; skip=1; break;
			case '@':	skip=0; *t++='%'; break;
			case '"':
			case '>':
			case '<':
			case '\'':	skip=0; *t++='\\'; *t++=*f; break;
			default:	skip=0; *t++=*f; break;
			}
			f++;
		}
		*t++='@';
		*t++='\0';
	}
	if ((fl & 0x01) && (a->point))
		sprintf(buf+strlen(buf),"p%u.",a->point);
	if (fl & 0x02)
		sprintf(buf+strlen(buf),"f%u.",a->node);
	if (fl & 0x04)
		sprintf(buf+strlen(buf),"n%u.",a->net);
	if ((fl & 0x08) && (a->zone))
		sprintf(buf+strlen(buf),"z%u.",a->zone);
	buf[strlen(buf)-1]='\0';
	if (fl & 0x10)
	{
		if (a->domain)
			sprintf(buf+strlen(buf),".%s",a->domain);
	}
	if (fl & 0x20)
	{
		if (a->domain)
		{
			if ((fl & 0x10) == 0)
				sprintf(buf+strlen(buf),".%s",a->domain);
		}
		else
		{
			for (tmpl=nllist;tmpl;tmpl=tmpl->next)
				if (tmpl->addr->zone == a->zone) break;
			if (tmpl && tmpl->addr->domain)
				sprintf(buf+strlen(buf),".%s",
					tmpl->addr->domain);
			else
				sprintf(buf+strlen(buf),".fidonet");
		}
		for (dt=domtrans;dt;dt=dt->next)
		{
			q=buf+strlen(buf)-strlen(dt->ftndom);
			if ((q >= buf) && (strcasecmp(dt->ftndom,q) == 0))
			{
				strcpy(q,dt->intdom);
				break;
			}
		}
		if (dt == NULL) sprintf(buf+strlen(buf),".ftn");
	}
	if ((fl & 0x80) && (a->name))
		sprintf(buf+strlen(buf),">");
	return buf;
}

char *ascfnode(a,fl)
faddr *a;
int fl;
{
	static char buf[128];

	if (a == NULL)
	{
		strcpy(buf,"<none>");
		return buf;
	}

	buf[0]='\0';
	if ((fl & 0x40) && (a->name))
		sprintf(buf+strlen(buf),"%s of ",a->name);
	if ((fl & 0x08) && (a->zone))
		sprintf(buf+strlen(buf),"%u:",a->zone);
	if (fl & 0x04)
		sprintf(buf+strlen(buf),"%u/",a->net);
	if (fl & 0x02)
		sprintf(buf+strlen(buf),"%u",a->node);
	if ((fl & 0x01) && (a->point))
		sprintf(buf+strlen(buf),".%u",a->point);
	if ((fl & 0x10) && (a->domain))
		sprintf(buf+strlen(buf),"@%s",a->domain);
	return buf;
}

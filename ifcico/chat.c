#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include "lutil.h"
#include "xutil.h"
#include "nodelist.h"
#include "config.h"

int chat(node*,modem_string*,modem_string*,modem_string*,int,char*);

struct _strlist {
	struct _strlist *next;
	char *string;
};

char *get_modem_string(modem_string*,node*);
char *next_modem_string(node*);
struct _strlist *sel_str(modem_string*,node*);
void tidy_str(struct _strlist *);

void tidy_str(sl)
struct _strlist *sl;
{
	struct _strlist *tmp;

	while (sl)
	{
		tmp=sl->next;
		free(sl->string);
		free(sl);
		sl=tmp;
	}
}

struct _strlist *sel_str(msl,nlent)
modem_string *msl;
node *nlent;
{
	struct _strlist *sl=NULL,*tmp=NULL;
	char *p,*q;

	for (p=get_modem_string(msl,nlent);p;p=next_modem_string(nlent))
	{
		if (sl == NULL)
		{
			sl=(struct _strlist *)
				xmalloc(sizeof(struct _strlist));
			tmp=sl;
		}
		else
		{
			tmp->next=(struct _strlist *)
				xmalloc(sizeof(struct _strlist));
			tmp=tmp->next;
		}
		tmp->next=NULL;
		tmp->string=xmalloc(strlen(p)+1);
		q=tmp->string;
		while (*p)
		{
			if (*p == '\\') switch (*++p)
			{
			case '\0':	p--; break;
			case '\\':	*q++='\\'; break;
			case 'r':	*q++='\r'; break;
			case 'n':	*q++='\n'; break;
			case 't':	*q++='\t'; break;
			case 's':	*q++=' '; break;
			case ' ':	*q++=' '; break;
			case 'b':	*q++='\b'; break;
			}
			else *q++ = *p;
			p++;
		}
		*q='\0';
	}
	return sl;
}

char *tranphone(char*,node*);
char *tranphone(phone,nlent)
char *phone;
node *nlent;
{
	static char trp[32];
	char buf[32];
	char *tf,*p,*q;
	int ln;

	if (phone == NULL) return NULL;
	strncpy(trp,phone,sizeof(trp)-1);
	for (tf=get_modem_string(phonetrans,nlent);tf;
	     tf=next_modem_string(nlent))
	{
		strncpy(buf,tf,sizeof(buf)-1);
		if ((p=strchr(buf,'/'))) *p++='\0';
		else p=buf+strlen(buf);
		while (*p && isspace(*p)) p++;
		q=buf+strlen(buf)-1;
		while (*q && isspace(*q)) *q--='\0';
		q=p+strlen(p)-1;
		while (*q && isspace(*q)) *q--='\0';
		ln=strlen(buf);
		if (strncmp(phone,buf,ln) == 0)
		{
			strcpy(trp,p);
			strncat(trp,phone+ln,sizeof(trp)-strlen(q)-1);
			break;
		}
	}
	return trp;
}

int send_str(char *,char*,node*);
int send_str(str,phone,nlent)
char *str,*phone;
node *nlent;
{
	char *p;

	p=str;
	debug(18,"send_str \"%s\"",str);
	while (*p)
	{
		if (*p == '\\') switch (*++p)
		{
		case '\0':	p--; break;
		case '\\':	putchar('\\'); break;
		case 'r':	putchar('\r'); break;
		case 'n':	putchar('\n'); break;
		case 't':	putchar('\t'); break;
		case 'b':	putchar('\b'); break;
		case 's':	putchar(' '); break;
		case ' ':	putchar(' '); break;
		case 'd':	sleep(1); break;
		case 'p':	usleep(250000L); break;
		case 'D':	if (phone) fputs(phone,stdout); break;
		case 'T':	if (phone) fputs(tranphone(phone,nlent),stdout); break;
		default:	putchar(*p); break;
		}
		else putchar(*p);
		p++;
	}
	if (ferror(stdout))
	{
		logerr("$send_str error");
		return 1;
	}
	else return 0;
}

static int expired=0;

void almhdl(int);
void almhdl(sig)
int sig;
{
	expired=1;
	debug(18,"timeout");
	return;
}

int expect_str(struct _strlist *,struct _strlist *,int);
int expect_str(succs,errs,timeout)
struct _strlist *succs,*errs;
int timeout;
{
	int maxl,l,i,rc;
	char *buf;
	struct _strlist *ts;
	int matched=0,smatch=0,ematch=0,ioerror=0;

	maxl=0;
	for (ts=errs;ts;ts=ts->next) 
		if ((l=strlen(ts->string)) > maxl)
			maxl=l;
	for (ts=succs;ts;ts=ts->next) 
		if ((l=strlen(ts->string)) > maxl)
			maxl=l;
	buf=xmalloc(maxl+1);
	memset(buf,0,maxl+1);

	expired=0;
	signal(SIGALRM,almhdl);
	alarm(timeout);
	while (!matched && !expired && !ioerror && !feof(stdin))
	{
		for (i=0;i<maxl;i++) buf[i]=buf[i+1];
		if ((rc=read(0,&(buf[maxl-1]),1)) != 1)
		{
			logerr("$chat got read return %d",rc);
			ioerror=1;
		}
		if (expired) debug(18,"chat got TIMEOUT");
		else debug(18,"chat got '%s'",
			printablec(buf[maxl-1]));
		for (ts=errs;ts && !matched;ts=ts->next)
		if (strcmp(ts->string,buf+maxl-strlen(ts->string)) == 0)
		{
			matched=1;
			ematch=1;
			loginf("chat got \"%s\", aborting",ts->string);
		}
		for (ts=succs;ts && !matched;ts=ts->next)
		if (strcmp(ts->string,buf+maxl-strlen(ts->string)) == 0)
		{
			matched=1;
			smatch=1;
			loginf("chat got \"%s\", continue",ts->string);
		}
		if (expired) loginf("chat got timeout, aborting");
		else
		if (ferror(stdin)) loginf("chat got error, aborting");
		if (feof(stdin)) logerr("chat got EOF, aborting");
	}
	alarm(0);
	signal(SIGALRM,SIG_DFL);

	rc=!(matched && smatch);
	free(buf);
	return rc;
}

int chat(nlent,send,success,error,timeout,phone)
node *nlent;
modem_string *send;
modem_string *success;
modem_string *error;
int timeout;
char *phone;
{
	char *sends;
	struct _strlist *succs=NULL, *errs=NULL;
	int rc;

	if ((sends=get_modem_string(send,nlent)) == NULL)
	{
		logerr("chat cannot find appropriate send string");
		return 1;
	}
	if ((success) && ((succs=sel_str(success,nlent)) == NULL))
	{
		logerr("chat cannot find appropriate expect string");
		return 1;
	}
	if ((error) && ((errs=sel_str(error,nlent)) == NULL))
	{
		tidy_str(succs);
		logerr("chat cannot find appropriate error string");
		return 1;
	}

	rc=send_str(sends,phone,nlent);
	if ((rc == 0) && (errs) && (succs)) 
		rc=expect_str(succs,errs,timeout);

	tidy_str(succs);
	tidy_str(errs);
	return rc;
}

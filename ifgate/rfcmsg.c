#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "xutil.h"
#include "lutil.h"
#include "rfcmsg.h"

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#ifndef BUFSIZ
#define BUFSIZ 256
#endif

#define KWDCHARS "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"

extern char *bgets(char *,int,FILE *);

rfcmsg *parsrfc(fp)
FILE *fp;
{
	int	linecont=FALSE,newcont,firstline;
	rfcmsg	*start=NULL, *cur=NULL;
	char	buffer[BUFSIZ];
	char	*p;

	while(bgets(buffer,BUFSIZ-1,fp) && strcmp(buffer,"\n"))
	{
		newcont=(buffer[strlen(buffer)-1] != '\n');
		debug(17,"Line read: \"%s\" - %s continued",
			buffer,newcont?"to be":"not to be");
		if (linecont)
		{
			debug(17,"this is a continuation of a long line");
			cur->val=xstrcat(cur->val,buffer);
		}
		else
		{
			if (isspace(buffer[0]))
			{
				if (strspn(buffer," \t\n") == strlen(buffer))
				{
					debug(17,"breaking with blank-only line");
					break;
				}
				debug(17,"this is a continuation line");
				if (!cur)
				{
					debug(17,"Wrong first line: \"%s\"",buffer);
					cur=(rfcmsg *)
						xmalloc(sizeof(rfcmsg));
					start=cur;
					cur->next=NULL;
					cur->key=xstrcpy("X-Body-Start");
					cur->val=xstrcpy(buffer);
					break;
				}
				else cur->val=xstrcat(cur->val,buffer);
			}
			else
			{
				debug(17,"this is a header line");
				if (cur)
				{
					firstline=FALSE;
					(cur->next)=(rfcmsg *)
						xmalloc(sizeof(rfcmsg));
					cur=cur->next;
				}
				else
				{
					firstline=TRUE;
					cur=(rfcmsg *)
						xmalloc(sizeof(rfcmsg));
					start=cur;
				}
				cur->next=NULL;
				cur->key=NULL;
				cur->val=NULL;
				if (firstline && !strncmp(buffer,"From ",5))
				{
					debug(17,"This is a uucpfrom line");
					cur->key=xstrcpy("X-UUCP-From");
					cur->val=xstrcpy(buffer+4);
				}
				else if ((p=strchr(buffer,':')) &&
				         (p > buffer) && /* ':' isn't 1st chr */
					 isspace(*(p+1)) && /* space past ':' */
				         (strspn(buffer,KWDCHARS) == (p-buffer)))
				{
					debug(17,"This is a regular header");
					*p='\0';
					cur->key=xstrcpy(buffer);
					cur->val=xstrcpy(p+1);
				}
				else 
				{
					debug(17,"Non-header line: \"%s\"",buffer);
					cur->key=xstrcpy("X-Body-Start");
					cur->val=xstrcpy(buffer);
					break;
				}
			}
		}
		linecont=newcont;
	}
	return(start);
}

void tidyrfc(msg)
rfcmsg *msg;
{
	rfcmsg *nxt;

	for (;msg;msg=nxt)
	{
		nxt=msg->next;
		if (msg->key) free(msg->key);
		if (msg->val) free(msg->val);
		free(msg);
	}
	return;
}

void dumpmsg(msg,fp)
rfcmsg *msg;
FILE *fp;
{
	char *p;

	p=hdr("X-Body-Start",msg);
	for (;msg;msg=msg->next) if (strcasecmp(msg->key,"X-Body-Start"))
	{
		if (!strcasecmp(msg->key,"X-UUCP-From"))
			fputs("From",fp);
		else
		{
			fputs(msg->key,fp);
			fputs(":",fp);
		}
		fputs(msg->val,fp);
	}
	fputs("\n",fp);
	if (p) fputs(p,fp);
	return;
}

char *hdr(key,msg)
char *key;
rfcmsg *msg;
{
	for (;msg;msg=msg->next)
		if (!strcasecmp(key,msg->key)) return(msg->val);
	return(NULL);
}

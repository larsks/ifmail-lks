#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "xutil.h"
#include "lutil.h"
#include "rfcmsg.h"
#include "config.h"
#include "bwrite.h"
#include "version.h"
#include "hash.h"

#define TEXTBODY 0
#define NONTEXTBODY 1
#define MESSAGEBODY 2
#define MULTIPARTBODY 3

extern char *bgets(char *,int,FILE *);

void putbody(level,msg,fp,pkt,forbidsplit,hdrsize)
int level;
rfcmsg *msg;
FILE *fp;
FILE *pkt;
int forbidsplit;
int hdrsize;
{
	int splitpart=0;
	int needsplit=0;
	int datasize=0;
	char buf[BUFSIZ];
	char *p;
	int bodytype=TEXTBODY;

	if (level == 0) {
		splitpart=0;
		needsplit=0;
	}

	if (needsplit) {
		fprintf(pkt," * Continuation %d of a split message *\r\r",
				splitpart);
			needsplit=0;
	}
	switch (bodytype) {
	case TEXTBODY:
		if ((p=hdr("X-Body-Start",msg)) && !needsplit) {
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
		break;
	}
	if (needsplit) {
		fprintf(pkt,"\r * Message split, to be continued *\r");
		splitpart++;
	}
}

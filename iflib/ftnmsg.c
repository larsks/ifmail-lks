#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "lutil.h"
#include "bwrite.h"
#include "ftn.h"
#include "ftnmsg.h"

extern FILE *openpkt(FILE *,faddr *,char);

static char *months[] = {
"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"
};

char *ftndate(t)
time_t t;
{
	static char buf[32];
	struct tm *ptm;

	ptm=localtime(&t);
	sprintf(buf,"%02d %s %02d  %02d:%02d:%02d",ptm->tm_mday,
		months[ptm->tm_mon],ptm->tm_year%100,
		ptm->tm_hour,ptm->tm_min,ptm->tm_sec);
	return buf;
}

FILE *ftnmsghdr(fmsg,pkt,route,flavor)
ftnmsg *fmsg;
FILE *pkt;
faddr *route;
char flavor;
{
	if (route == NULL) route=fmsg->to;
	pkt=openpkt(pkt,route,flavor);
	if (pkt == NULL) return NULL;

	iwrite(MSGTYPE,         pkt);
#ifndef USE_REAL_BINARY_FROM
	if (fmsg->area)
		iwrite(bestaka_s(fmsg->to)->node,pkt);
	else
#endif
		iwrite(fmsg->from->node,pkt);
	iwrite(fmsg->to->node,  pkt);
#ifndef USE_REAL_BINARY_FROM
	if (fmsg->area)
		iwrite(bestaka_s(fmsg->to)->net,pkt);
	else
#endif
		iwrite(fmsg->from->net, pkt);
	iwrite(fmsg->to->net,   pkt);
	iwrite(fmsg->flags,     pkt);
	iwrite(0,               pkt);
	awrite(ftndate(fmsg->date),pkt);
	awrite(fmsg->to->name?fmsg->to->name:"Sysop",  pkt);
	awrite(fmsg->from->name?fmsg->from->name:"Sysop",pkt);
	awrite(fmsg->subj?fmsg->subj:"<none>",pkt);

	if (fmsg->area)
	{
		fprintf(pkt,"AREA:%s\r",fmsg->area);
	}
	else
	{
		if (fmsg->to->point)
			fprintf(pkt,"\1TOPT %u\r",fmsg->to->point);
		if (fmsg->from->point)
			fprintf(pkt,"\1FMPT %u\r",fmsg->from->point);
#ifndef FORCEINTL
		if (fmsg->to->zone != fmsg->from->zone)
#endif
			fprintf(pkt,"\1INTL %u:%u/%u %u:%u/%u\r",
				fmsg->to->zone,
				fmsg->to->net,
				fmsg->to->node,
				fmsg->from->zone,
				fmsg->from->net,
				fmsg->from->node
				);
	}

	if (fmsg->msgid_s) 
		fprintf(pkt,"\1MSGID: %s\r",fmsg->msgid_s);
	else if (fmsg->msgid_a) 
		fprintf(pkt,"\1MSGID: %s %08lx\r",
			fmsg->msgid_a,
			fmsg->msgid_n);

	if (fmsg->reply_s) 
		fprintf(pkt,"\1REPLY: %s\r",fmsg->reply_s);
	else if (fmsg->reply_a)
		fprintf(pkt,"\1REPLY: %s %08lx\r",
			fmsg->reply_a,
			fmsg->reply_n);

	if (ferror(pkt)) return NULL;
	else return pkt;
}

void tidy_ftnmsg(tmsg)
ftnmsg *tmsg;
{
	if (tmsg == NULL) return;

	tmsg->flags=0;
	if (tmsg->to) tidy_faddr(tmsg->to); tmsg->to=NULL;
	if (tmsg->from) tidy_faddr(tmsg->from); tmsg->from=NULL;
	if (tmsg->subj) free(tmsg->subj); tmsg->subj=NULL;
	if (tmsg->msgid_s) free(tmsg->msgid_s); tmsg->msgid_s=NULL;
	if (tmsg->msgid_a) free(tmsg->msgid_a); tmsg->msgid_a=NULL;
	if (tmsg->reply_s) free(tmsg->reply_s); tmsg->reply_s=NULL;
	if (tmsg->reply_a) free(tmsg->reply_a); tmsg->reply_a=NULL;
	if (tmsg->origin) free(tmsg->origin); tmsg->origin=NULL;
	if (tmsg->area) free(tmsg->area); tmsg->area=NULL;
	free(tmsg);
}

#include <stdio.h>
#include <string.h>
#include "lutil.h"
#include "ftn.h"
#include "bread.h"
#include "config.h"

faddr pktfrom;
char pktpwd[9];

int getheader(f,t,pkt)
faddr *f,*t;
FILE *pkt;
{
	int i,pwdok;
	int capword,capvalid;
	int tome,pointcheck;
	fa_list *al;
	char *p;

	f->domain=NULL;
	f->name=NULL;
	t->domain=NULL;
	t->name=NULL;

	debug(5,"from node: %u",f->node=iread(pkt));
	debug(5,"to   node: %u",t->node=iread(pkt));
	debug(5,"year     : %u",iread(pkt));
	debug(5,"month    : %u",iread(pkt));
	debug(5,"day      : %u",iread(pkt));
	debug(5,"hour     : %u",iread(pkt));
	debug(5,"min      : %u",iread(pkt));
	debug(5,"sec      : %u",iread(pkt));
	debug(5,"rate     : %u",iread(pkt));
	debug(5,"ver      : %u",iread(pkt));
	debug(5,"from net : %u",f->net=iread(pkt));
	debug(5,"to   net : %u",t->net=iread(pkt));
	debug(5,"prodx    : 0x%04x",iread(pkt));
	for (i=0;i<8;i++) pktpwd[i]=getc(pkt);
	pktpwd[8]='\0';
	for (p=pktpwd+7;(p >= pktpwd) && (*p == ' ');p--) *p='\0';
	debug(5,"password : %s",pktpwd);
	if (pktpwd[0]) f->name=pktpwd;
	debug(5,"from zone: %u",f->zone=iread(pkt));
	debug(5,"to   zone: %u",t->zone=iread(pkt));
	debug(5,"filler   : 0x%04x",iread(pkt));
	debug(5,"capvalid : 0x%04x",capvalid=iread(pkt));
	debug(5,"prodcode : 0x%04x",iread(pkt));
	debug(5,"capword  : 0x%04x",capword=iread(pkt));
	debug(5,"from zone: %u",iread(pkt));
	debug(5,"to   zone: %u",iread(pkt));
	debug(5,"from pnt : %u",f->point=iread(pkt));
	debug(5,"to   pnt : %u",t->point=iread(pkt));
	debug(5,"proddata : 0x%08lx",lread(pkt));

	if (feof(pkt) || ferror(pkt)) 
	{
		logerr("$Could not read packet header");
		return 2;
	}

	pointcheck=0;
	if (((capword >> 8) == (capvalid & 0xff)) &&
	    ((capvalid >> 8) == (capword & 0xff)))
		pointcheck=(capword & 0x0001);
	else capword=0;

	debug(5,"capword=%04x, pointcheck=%s",capword,pointcheck?"yes":"no");

	pktfrom.name=NULL;
	pktfrom.domain=NULL;
	pktfrom.zone=f->zone;
	pktfrom.net=f->net;
	pktfrom.node=f->node;
	if (pointcheck) pktfrom.point=f->point;
	else pktfrom.point=0;

	tome=0;
	for (al=whoami;al;al=al->next)
	{
	if (((t->zone == 0) || (t->zone == al->addr->zone)) &&
	    (t->net == al->addr->net) &&
	    (t->node == al->addr->node) &&
	    ((!pointcheck) || (t->point == al->addr->point)))
		tome=1;
	}

	pwdok=1;
	for (al=pwlist;al;al=al->next)
	{
	if (((f->zone == 0) || (f->zone == al->addr->zone)) &&
	    (f->net == al->addr->net) &&
	    (f->node == al->addr->node) &&
	    ((!pointcheck) || (f->point == al->addr->point)))
		if (strcasecmp(al->addr->name,pktpwd) != 0)
		{
			pwdok=0;
			loginf("password got \"%s\", expected \"%s\"",
				pktpwd,al->addr->name);
		}
	}

	loginf("packet from node %s",ascfnode(f,0x1f));
	loginf("         to node %s",ascfnode(t,0x1f));

	if (!tome) return 3;
	else if (!pwdok) return 4;
	else return 0;
}

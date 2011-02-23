#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "ftn.h"
#include "xutil.h"
#include "lutil.h"
#include "emsi.h"
#include "session.h"
#include "config.h"
#include "version.h"
#include "falists.h"

extern time_t mtime2sl(time_t);
extern time_t sl2mtime(time_t);
extern int nodelock(faddr*);

char *mkemsidat(caller)
int caller;
{
	time_t tt;
	char cbuf[16];
	char *p;
	fa_list *tmp;
	faddr *primary;

	p=xstrcpy("EMSI_DAT0000{EMSI}{");

	primary=bestaka_s(remote->addr);
	p=xstrcat(p,ascfnode(primary,0x1f));

	for (tmp=whoami;tmp;tmp=tmp->next)
	if (tmp->addr != primary)
	{
		p=xstrcat(p," ");
		p=xstrcat(p,ascfnode(tmp->addr,0x1f));
	}
	p=xstrcat(p,"}{");
	if (emsi_local_password) p=xstrcat(p,emsi_local_password);
	else if (remote)
	for (tmp=pwlist;tmp;tmp=tmp->next)
	if (metric(remote->addr,tmp->addr) == 0)
	{
		p=xstrcat(p,tmp->addr->name);
		break;
	}
	p=xstrcat(p,"}{8N1");
	if (caller)
	{
		     if (emsi_local_lcodes & PUA) p=xstrcat(p,",PUA");
		else if (emsi_local_lcodes & PUP) p=xstrcat(p,",PUP");
		else if (emsi_local_lcodes & NPU) p=xstrcat(p,",NPU");
	}
	else
	{
		if (emsi_local_lcodes & HAT) p=xstrcat(p,",HAT");
		if (emsi_local_lcodes & HXT) p=xstrcat(p,",HXT");
		if (emsi_local_lcodes & HRQ) p=xstrcat(p,",HRQ");
	}
	p=xstrcat(p,"}{");
	if (emsi_local_protos & DZA) p=xstrcat(p,"DZA,");
	if (emsi_local_protos & ZAP) p=xstrcat(p,"ZAP,");
	if (emsi_local_protos & ZMO) p=xstrcat(p,"ZMO,");
	if (emsi_local_protos & JAN) p=xstrcat(p,"JAN,");
	if (emsi_local_protos & HYD) p=xstrcat(p,"HYD,");
	if (emsi_local_protos & KER) p=xstrcat(p,"KER,");
	if (emsi_local_protos & TCP) p=xstrcat(p,"TCP,");
	if (emsi_local_protos ==  0) p=xstrcat(p,"NCP,");
	if (emsi_local_opts & NRQ) p=xstrcat(p,"NRQ,");
	if (emsi_local_opts & ARC) p=xstrcat(p,"ARC,");
	if (emsi_local_opts & XMA) p=xstrcat(p,"XMA,");
	if (emsi_local_opts & FNC) p=xstrcat(p,"FNC,");
	if (emsi_local_opts & CHT) p=xstrcat(p,"CHT,");
	if (emsi_local_opts & SLK) p=xstrcat(p,"SLK,");
	if (*(p+strlen(p)-1) == ',') *(p+strlen(p)-1) = '}';
	else p=xstrcat(p,"}");
	sprintf(cbuf,"{%X}",PRODCODE);
	p=xstrcat(p,cbuf);
	p=xstrcat(p,"{ifcico}{");
	p=xstrcat(p,version);
	p=xstrcat(p,"}{");
	p=xstrcat(p,reldate);
	p=xstrcat(p,"}{TRX#}{[");
	(void)time(&tt);
	sprintf(cbuf,"%08lX",mtime2sl(tt));
	p=xstrcat(p,cbuf);
	p=xstrcat(p,"]}{IDENT}{[");
	p=xstrcat(p,name?name:"Unknown");
	p=xstrcat(p,"][");
	p=xstrcat(p,location?location:"Unknown");
	p=xstrcat(p,"][");
	p=xstrcat(p,sysop?sysop:"Unknown");
	p=xstrcat(p,"][");
	p=xstrcat(p,phone?phone:"-Unpublished-");
	p=xstrcat(p,"][");
	if (speed) sprintf(cbuf,"%ld",speed);
	else strcpy(cbuf,"9600");
	p=xstrcat(p,cbuf);
	p=xstrcat(p,"][");
	p=xstrcat(p,flags?flags:"");
	p=xstrcat(p,"]}");

	sprintf(cbuf,"%04X",strlen(p+12));
	memcpy(p+8,cbuf,4);
	debug(10,"prepared: \"%s\"",p);
	return p;
}

char *sel_brace(char*);
char *sel_brace(s)
char *s;
{
	static char *save;
	char *p,*q;
	int i;

	if (s == NULL) s=save;
	for (;*s && (*s != '{');s++);
	if (*s == '\0')
	{
		save=s;
		return NULL;
	}
	else s++;
	for (p=s,q=s;*p;p++) switch (*p)
	{
	case '}':	if (*(p+1) == '}') *q++=*p++;
			else
			{
				*q='\0';
				save=p+1;
				goto exit;
			}
			break;
	case '\\':	if (*(p+1) == '\\') *q++=*p++;
			else
			{
				sscanf(p+1,"%02x",&i);
				*q++=i;
				p+=2;
			}
			break;
	default:	*q++=*p;
			break;
	}
exit:
	return s;
}

char *sel_bracket(char*);
char *sel_bracket(s)
char *s;
{
	static char *save;
	char *p,*q;
	int i;

	if (s == NULL) s=save;
	for (;*s && (*s != '[');s++);
	if (*s == '\0')
	{
		save=s;
		return NULL;
	}
	else s++;
	for (p=s,q=s;*p;p++) switch (*p)
	{
	case ']':	if (*(p+1) == ']') *q++=*p++;
			else
			{
				*q='\0';
				save=p+1;
				goto exit;
			}
			break;
	case '\\':	if (*(p+1) == '\\') *q++=*p++;
			else
			{
				sscanf(p+1,"%02x",&i);
				*q++=i;
				p+=2;
			}
			break;
	default:	*q++=*p;
			break;
	}
exit:
	return s;
}

int scanemsidat(buf)
char *buf;
{
	char *p,*q;
	fa_list **tmp,*tmpa;
	faddr *fa;
	char *mailer_prod,*mailer_name,*mailer_version,*mailer_serial;

	debug(10,"got data packet: \"%s\"",buf);

	p=sel_brace(buf);
	if (strcasecmp(p,"EMSI") != 0)
	{
		loginf("This can never occur. Got \"%s\" instead of \"EMSI\"",p);
		return 1;
	}
	p=sel_brace(NULL);

/*	tidy_falist(&remote);
	remote=NULL;
	tmp=&remote;
 hmm, I've been reported that this may cause trouble.  If we are calling to
 a node that does not send its nodelist address in the emsi packet, packets
 to this address will never be delivered.  So, we need to take care of
 the address *we* know, add any addresses sent to us in emsi_dat and remove
 duplicates.  The latter is useful anyway...
*/
	for (tmp=&remote;*tmp;tmp=&((*tmp)->next)) /*nothing*/ ;
/* yes, feels better... tmp points on the NULL pointer at the end */
	for (q=strtok(p," ");q;q=strtok(NULL," "))
	if ((fa=parsefnode(q)))
	{
		*tmp=(fa_list*)xmalloc(sizeof(fa_list));
		(*tmp)->next=NULL;
		(*tmp)->addr=fa;
		tmp=&((*tmp)->next);
	}

/* and now, have to eliminate duplicates */
	sort_list(&remote);
	uniq_list(&remote);
/* well, should be OK now.  See what happens... */

	for (tmpa=remote;tmpa;tmpa=tmpa->next)
	{
		loginf("remote  address: %s",ascfnode(tmpa->addr,0x1f));
		(void)nodelock(tmpa->addr);
	}

	if (emsi_remote_password) free(emsi_remote_password);
	emsi_remote_password=xstrcpy(sel_brace(NULL));
#ifdef SUPPRESS_PASSWORD_LOGGING
	loginf("remote password: %s",
		emsi_remote_password?"*SUPPRESSED*":"(none)");
#else
	loginf("remote password: %s",
		emsi_remote_password?emsi_remote_password:"(none)");
#endif

	p=sel_brace(NULL);
	for (q=strtok(p,",");q;q=strtok(NULL,","))
	{
		if (((q[0] >= '5') && (q[0] <= '8')) &&
		    ((toupper(q[1]) == 'N') ||
		     (toupper(q[1]) == 'O') ||
		     (toupper(q[1]) == 'E') ||
		     (toupper(q[1]) == 'S') ||
		     (toupper(q[1]) == 'M')) &&
		    ((q[2] == '1') || (q[2] == '2')))
		{
			strncpy(emsi_remote_comm,q,3);
		}
		else if (strcasecmp(q,"PUA") == 0) emsi_remote_lcodes |= PUA;
		else if (strcasecmp(q,"PUP") == 0) emsi_remote_lcodes |= PUP;
		else if (strcasecmp(q,"NPU") == 0) emsi_remote_lcodes |= NPU;
		else if (strcasecmp(q,"HAT") == 0) emsi_remote_lcodes |= HAT;
		else if (strcasecmp(q,"HXT") == 0) emsi_remote_lcodes |= HXT;
		else if (strcasecmp(q,"HRQ") == 0) emsi_remote_lcodes |= HRQ;
		else loginf("unrecognized EMSI link code: \"%s\"",q);
	}

	p=sel_brace(NULL);
	for (q=strtok(p,",");q;q=strtok(NULL,","))
	{
		     if (strcasecmp(q,"DZA") == 0) emsi_remote_protos |= DZA;
		else if (strcasecmp(q,"ZAP") == 0) emsi_remote_protos |= ZAP;
		else if (strcasecmp(q,"ZMO") == 0) emsi_remote_protos |= ZMO;
		else if (strcasecmp(q,"JAN") == 0) emsi_remote_protos |= JAN;
		else if (strcasecmp(q,"HYD") == 0) emsi_remote_protos |= HYD;
		else if (strcasecmp(q,"KER") == 0) emsi_remote_protos |= KER;
		else if (strcasecmp(q,"TCP") == 0) emsi_remote_protos |= TCP;
		else if (strcasecmp(q,"NCP") == 0) emsi_remote_protos = 0;
		else if (strcasecmp(q,"NRQ") == 0) emsi_remote_opts |= NRQ;
		else if (strcasecmp(q,"ARC") == 0) emsi_remote_opts |= ARC;
		else if (strcasecmp(q,"XMA") == 0) emsi_remote_opts |= XMA;
		else if (strcasecmp(q,"FNC") == 0) emsi_remote_opts |= FNC;
		else if (strcasecmp(q,"CHT") == 0) emsi_remote_opts |= CHT;
		else if (strcasecmp(q,"SLK") == 0) emsi_remote_opts |= SLK;
		else loginf("unrecognized EMSI proto/option code: \"%s\"",q);
	}
	if ((emsi_remote_opts & FNC) == 0) remote_flags &= ~SESSION_FNC;
	mailer_prod=sel_brace(NULL);
	mailer_name=sel_brace(NULL);
	mailer_version=sel_brace(NULL);
	mailer_serial=sel_brace(NULL);
	loginf("remote     uses: %s [%s] version %s/%s",
		mailer_name,mailer_prod,mailer_version,mailer_serial);
	while ((p=sel_brace(NULL)))
	if (strcasecmp(p,"IDENT") == 0)
	{
		p=sel_brace(NULL);
		loginf("remote   system: %s",sel_bracket(p));
		loginf("remote location: %s",sel_bracket(NULL));
		loginf("remote operator: %s",(p=sel_bracket(NULL)));
		if (remote && remote->addr)
			remote->addr->name=xstrcpy(p);
		loginf("remote    phone: %s",sel_bracket(NULL));
		loginf("remote     baud: %s",sel_bracket(NULL));
		loginf("remote    flags: %s",sel_bracket(NULL));
	}
	else if (strcasecmp(p,"TRX#") == 0)
	{
		time_t tt;
		char ctt[32];

		p=sel_brace(NULL);
		p=sel_bracket(p);
		if (sscanf(p,"%08lx",&tt) == 1)
		{
			strcpy(ctt,date(sl2mtime(tt)));
			loginf("remote     time: %s",ctt);
		}
		else
			loginf("remote     TRX#: %s",p);
	}
	else if (strcasecmp(p, "TRAF") == 0)
	{
		u_long tt, tt1;

		p=sel_brace(NULL);
		if (sscanf(p, "%08lx %08lx", &tt, &tt1) == 2) {
			loginf("remote  netmail: %u byte(s)", tt);
			loginf("remote echomail: %u byte(s)", tt1);
		} else
			loginf("remote     TRAF: %s", p);
	}
	else if (strcasecmp(p, "MOH#") == 0)
	{
		u_long tt;

		p=sel_brace(NULL);
		p=sel_bracket(p);
		if (sscanf(p, "%08lx", &tt) == 1)
			loginf("remote    files: %u byte(s)", tt);
		else
			loginf("remote     MOH#: %s",p);
	}
	else
	{
		q=sel_brace(NULL);
		loginf("remote tag: \"%s\" value: \"%s\"",p,q);
	}

	return 0;
}

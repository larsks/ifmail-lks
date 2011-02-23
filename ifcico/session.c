#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef HAS_TCP
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include "ttyio.h"
#include "lutil.h"
#include "xutil.h"
#include "ftn.h"
#include "nodelist.h"
#include "statetbl.h"
#include "session.h"
#include "config.h"
#include "emsi.h"
#include "version.h"

extern int nodeulock(faddr*);

node *nlent;
fa_list *remote=NULL;
int session_flags;
int remote_flags;

int tx_define_type();
int rx_define_type();

static int type;
static char *data=NULL;

int session(a,nl,role,tp,dt)
faddr *a;
node *nl;
int role;
int tp;
char *dt;
{
	int rc=1;
	fa_list *tmpl;
#ifdef HAS_TCP
	struct sockaddr_in peeraddr;
	int addrlen=sizeof(struct sockaddr_in);
#endif

	session_flags=0;
	type=tp;
	nlent=nl;

	debug(10,"start %s session type %d with %s",
		role?"master":"slave",type,ascfnode(a,0x1f));

#ifdef HAS_TCP
	if (getpeername(0,(struct sockaddr*)&peeraddr,&addrlen) == 0)
	{
		debug(10,"%s TCP connection: len=%d, family=%hd, port=%hu, addr=%s",
			role?"outgoing":"incoming",
			addrlen,peeraddr.sin_family,
			peeraddr.sin_port,
			inet_ntoa(peeraddr.sin_addr));
		if (role == 0)
			loginf("incoming TCP connection from %s",
			inet_ntoa(peeraddr.sin_addr));
		session_flags |= SESSION_TCP;
	}
#endif

	if (data) free(data);
	data=NULL;
	if (dt) data=xstrcpy(dt);

	emsi_local_protos=0;
	emsi_local_opts=0;
	emsi_local_lcodes=0;
	
	tidy_falist(&remote);
	remote=NULL;
	if (a)
	{
		remote=(fa_list*)xmalloc(sizeof(fa_list));
		remote->next=NULL;
		remote->addr=(faddr*)xmalloc(sizeof(faddr));
		remote->addr->zone=a->zone;
		remote->addr->net=a->net;
		remote->addr->node=a->node;
		remote->addr->point=a->point;
		remote->addr->domain=xstrcpy(a->domain);
		remote->addr->name=NULL;
	}
	else remote=NULL;

	remote_flags=SESSION_FNC;

	if (role)
	{
		if (type == SESSION_UNKNOWN) (void)tx_define_type();
		switch(type)
		{
		case SESSION_UNKNOWN:	rc=20; break;
		case SESSION_FTSC:	rc=tx_ftsc(); break;
		case SESSION_YOOHOO:	rc=tx_yoohoo(); break;
		case SESSION_EMSI:	rc=tx_emsi(data); break;
		}
	}
	else
	{
		if (type == SESSION_FTSC) session_flags |= FTSC_XMODEM_CRC;
		if (type == SESSION_UNKNOWN) (void)rx_define_type();
		switch(type)
		{
		case SESSION_UNKNOWN:	rc=20; break;
		case SESSION_FTSC:	rc=rx_ftsc(); break;
		case SESSION_YOOHOO:	rc=rx_yoohoo(); break;
		case SESSION_EMSI:	rc=rx_emsi(data); break;
		}
	}
	sleep(2);
	for (tmpl=remote;tmpl;tmpl=tmpl->next)
		(void)nodeulock(tmpl->addr);
		/* unlock all systems, locks not owned by us are untouched */

	return rc;
}

SM_DECL(tx_define_type,"tx_define_type")
SM_STATES
	skipjunk,wake,waitchar,nextchar,checkintro,sendinq
SM_NAMES
	"skipjunk","wake","waitchar","nextchar","checkintro","sendinq"
SM_EDECL

	int c=0;
	int count=0;
	char buf[256],*p;
	char ebuf[13],*ep;
	int standby=0;

	int maybeftsc=0;
	int maybeyoohoo=0;

	type=SESSION_UNKNOWN;
	ebuf[0]='\0';
	ep=ebuf;
	buf[0]='\0';
	p=buf;

SM_START(skipjunk)

SM_STATE(skipjunk)

	while ((c=GETCHAR(1)) >= 0) /*nothing*/ ;
	if (c == TIMEOUT) {SM_PROCEED(wake);}
	else {SM_ERROR;}

SM_STATE(wake)

	if (count++ > 20)
	{
		loginf("remote did not respond");
		SM_ERROR;
	}

	p=buf;
	/*PUTSTR(" \r");*/
	PUTCHAR('\r');
	if ((c=GETCHAR(2)) == TIMEOUT)
	{
		SM_PROCEED(wake);
	}
	else if (c < 0)
	{
		loginf("error while waking remote");
		SM_ERROR;
	}
	else
	{
		count=0;
		SM_PROCEED(nextchar);
	}

SM_STATE(waitchar)

	if ((c=GETCHAR(4)) == TIMEOUT)
	{
		standby=0;
		ep=ebuf;
		ebuf[0]='\0';
		if (count++ > 8)
		{
			loginf("too many tries waking remote");
			SM_ERROR;
		}
		SM_PROCEED(sendinq);
	}
	else if (c < 0)
	{
		loginf("error while getting intro from remote");
		SM_ERROR;
	}
	else
	{
		SM_PROCEED(nextchar);
	}

SM_STATE(nextchar)

	if (c == 'C')
	{
		session_flags |= FTSC_XMODEM_CRC;
		maybeftsc++;
	}
	if (c == NAK)
	{
		session_flags &= ~FTSC_XMODEM_CRC;
		maybeftsc++;
	}
	if (c == ENQ) maybeyoohoo++;

	if (((localoptions & NOWAZOO) == 0) &&
	    (maybeyoohoo > 1))
	{
		type=SESSION_YOOHOO;
		SM_SUCCESS;
	}

	if (maybeftsc > 1)
	{
		type=SESSION_FTSC;
		SM_SUCCESS;
	}

	if ((c >= ' ') && (c <= '~'))
	{
		if (c != 'C') maybeftsc=0;
		maybeyoohoo=0;

		if ((p-buf) < (sizeof(buf)-1))
		{
			*p++=c;
			*p='\0';
		}

		if (c == '*')
		{
			standby=1;
			ep=ebuf;
			buf[0]='\0';
		}
		else if (standby)
		{
			if ((ep-ebuf) < (sizeof(ebuf)-1))
			{
				*ep++=c;
				*ep='\0';
			}
			if ((ep-ebuf) >= (sizeof(ebuf)-1))
			{
				standby=0;
				SM_PROCEED(checkintro);
			}
		}
	}
	else switch (c)
	{
	case DC1:	break;
	case '\r':
	case '\n':	standby=0;
			ep=ebuf;
			ebuf[0]='\0';
			if (buf[0]) loginf("Intro: \"%s\"",buf);
			p=buf;
			buf[0]='\0';
			break;
	default:	standby=0;
			ep=ebuf;
			ebuf[0]='\0';
			debug(10,"got '%s' reading intro",
				printablec(c));
			break;
	}

	SM_PROCEED(waitchar);

SM_STATE(checkintro)

	debug(10,"check \"%s\" for being EMSI request",ebuf);

	if (((localoptions & NOEMSI) == 0) &&
	    (strncasecmp(ebuf,"EMSI_REQA77E",12) == 0))
	{
		type=SESSION_EMSI;
		data=xstrcpy("**EMSI_REQA77E");
		sleep(2);
		PUTSTR("**EMSI_INQC816\r**EMSI_INQC816\r\021");
		SM_SUCCESS;
	}
	else
	{
		p=buf;
		buf[0]='\0';
		SM_PROCEED(waitchar);
	}

SM_STATE(sendinq)

	PUTCHAR(DC1);
	if ((localoptions & NOEMSI) == 0)
		PUTSTR("\r**EMSI_INQC816**EMSI_INQC816");
	if ((localoptions & NOWAZOO) == 0)
		PUTCHAR(YOOHOO);
	PUTCHAR(TSYNC);
	if ((localoptions & NOEMSI) == 0)
		PUTSTR("\r\021");
	SM_PROCEED(waitchar);

SM_END
SM_RETURN


SM_DECL(rx_define_type,"rx_define_type")
SM_STATES
	sendintro,waitchar,nextchar,checkemsi,getdat
SM_NAMES
	"sendintro","waitchar","nextchar","checkemsi","getdat"
SM_EDECL

	int count=0;
	int c=0;
	int maybeftsc=0,maybeyoohoo=0;
	char ebuf[13],*ep;
	char *p;
	int standby=0;
	int datasize;

	type=SESSION_UNKNOWN;
	session_flags|=FTSC_XMODEM_CRC;
	ebuf[0]='\0';
	ep=ebuf;


SM_START(sendintro)

SM_STATE(sendintro)

	if (count++ > 16)
	{
		loginf("too many tries to get anything from the caller");
		SM_ERROR;
	}

	standby=0;
	ep=ebuf;
	ebuf[0]='\0';

	if ((localoptions & NOEMSI) == 0)
	{
		PUTSTR("**EMSI_REQA77E\r\021");
	}
	PUTSTR("\r\rAddress: ");
	PUTSTR(ascfnode(whoami->addr,0x1f));
	PUTSTR(" using ifcico ");
	PUTSTR(version);
	PUTSTR(" of ");
	PUTSTR(reldate);
	PUTCHAR('\r');
	if (STATUS) {SM_ERROR;}
	else {SM_PROCEED(waitchar);}

SM_STATE(waitchar)

	if ((c=GETCHAR(8)) == TIMEOUT)
	{
		SM_PROCEED(sendintro);
	}
	else if (c < 0)
	{
		loginf("error while getting from the caller");
		SM_ERROR;
	}
	else
	{
		SM_PROCEED(nextchar);
	}

SM_STATE(nextchar)

	if ((c >= ' ') && (c <= 'z'))
	{
		if (c == '*')
		{
			standby=1;
			ep=ebuf;
			ebuf[0]='\0';
		}
		else if (standby)
		{
			if ((ep-ebuf) < (sizeof(ebuf)-1))
			{
				*ep++=c;
				*ep='\0';
			}
			if ((ep-ebuf) >= (sizeof(ebuf)-1))
			{
				standby=0;
				SM_PROCEED(checkemsi);
			}
		}
		SM_PROCEED(waitchar);
	}
	else switch (c)
	{
	case DC1:	SM_PROCEED(waitchar);
			break;
	case TSYNC:	standby=0;
			ep=ebuf;
			ebuf[0]='\0';
			if (++maybeftsc > 1)
			{
				type=SESSION_FTSC;
				SM_SUCCESS;
			}
			else {SM_PROCEED(waitchar);}
			break;
	case YOOHOO:	standby=0;
			ep=ebuf;
			ebuf[0]='\0';
			if (++maybeyoohoo > 1)
			{
				type=SESSION_YOOHOO;
				SM_SUCCESS;
			}
			else {SM_PROCEED(waitchar);}
			break;
	case '\r':
	case '\n':	standby=0;
			ep=ebuf;
			ebuf[0]='\0';
			if (ebuf[0]) {SM_PROCEED(checkemsi);}
			else {SM_PROCEED(sendintro);}
			break;
	default:	standby=0;
			ep=ebuf;
			ebuf[0]='\0';
			debug(10,"got '%s' from remote",
				printablec(c));
			SM_PROCEED(waitchar);
			break;
	}

SM_STATE(checkemsi)

	debug(10,"check \"%s\" for being EMSI inquery or data",ebuf);

	if (localoptions & NOEMSI)
	{
		SM_PROCEED(sendintro);
	}

	if (strncasecmp(ebuf,"EMSI_INQC816",12) == 0)
	{
		type=SESSION_EMSI;
		data=xstrcpy("**EMSI_INQC816");
		SM_SUCCESS;
	}
	else if (strncasecmp(ebuf,"EMSI_DAT",8) == 0)
	{
		SM_PROCEED(getdat);
	}
	else
	{
		SM_PROCEED(sendintro);
	}

SM_STATE(getdat)

	debug(10,"try get emsi_dat packet starting with \"%s\"",ebuf);

	if (sscanf(ebuf+8,"%04x",&datasize) != 1)
	{
		SM_PROCEED(sendintro);
	}

	datasize += 18; /* strlen("**EMSI_DATxxxxyyyy"), include CRC */
	data=xmalloc(datasize+1);
	strcpy(data,"**");
	strcat(data,ebuf);
	p=data+strlen(data);

	while (((p-data) < datasize) && ((c=GETCHAR(8)) >= 0))
	{
		*p++=c;
		*p='\0';
	}
	if (c == TIMEOUT)
	{
		SM_PROCEED(sendintro);
	}
	else if (c < 0)
	{
		loginf("error while reading EMSI_DAT from the caller");
		SM_ERROR;
	}
	type=SESSION_EMSI;
	SM_SUCCESS;

SM_END
SM_RETURN

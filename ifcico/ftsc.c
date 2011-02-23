#include <stdlib.h>
#include <stdio.h>
#include "ftn.h"
#include "getheader.h"
#include "xutil.h"
#include "lutil.h"
#include "session.h"
#include "ttyio.h"
#include "statetbl.h"
#include "config.h"

extern int master;
extern int nodelock(faddr*);

int rx_ftsc(void);
int tx_ftsc(void);

extern unsigned INT32 sequencer(void);
extern int xmrecv(char*);
extern int xmsend(char*,char*,int);
extern int xmsndfiles(file_list*);
extern int sendbark(void);
extern int recvbark(void);
extern void rdoptions(node*);

static int rxftsc(void);
static int txftsc(void);
static int recvfiles(void);
static file_list *tosend;

int rx_ftsc(void)
{
	int rc;

	loginf("start inbound ftsc session");
	session_flags |= SESSION_BARK;
	rc=rxftsc();
	if (rc)
	{
		loginf("inbound ftsc session failed, rc=%d",rc);
		PUTCHAR(CAN);
		PUTCHAR(CAN);
		PUTCHAR(CAN);
	}
	else debug(10,"inbound ftsc session rc=%d",rc);
	tidy_filelist(tosend,0);
	tosend=NULL;
	return rc;
}

int tx_ftsc(void)
{
	int rc;

	loginf("start outbound ftsc session with %s",
		ascfnode(remote->addr,0x1f));
	rc=txftsc();
	if (rc)
	{
		loginf("outbound ftsc session failed, rc=%d",rc);
		PUTCHAR(CAN);
		PUTCHAR(CAN);
		PUTCHAR(CAN);
	}
	else debug(10,"outbound ftsc session rc=%d",rc);
	tidy_filelist(tosend,0);
	tosend=NULL;
	return rc;
}

SM_DECL(txftsc,"txftsc")
SM_STATES
	wait_command,recv_mail,send_req,recv_req
SM_NAMES
	"wait_command","recv_mail","send_req","recv_req"
SM_EDECL

	int c,rc;
	char *nonhold_mail;

	if (localoptions & NOHOLD) nonhold_mail=ALL_MAIL;
	else nonhold_mail=NONHOLD_MAIL;
	tosend=create_filelist(remote,nonhold_mail,2);

	if ((rc=xmsndfiles(tosend))) return rc;

SM_START(wait_command)

SM_STATE(wait_command)

	c=GETCHAR(30);
	if (c == TIMEOUT)
	{
		loginf("timeout waiting for remote action, try receive");
		SM_PROCEED(recv_mail);
	}
	else if (c < 0)
	{
		loginf("got error waiting for TSYNC: received %d",c);
		SM_ERROR;
	}
	else switch (c)
	{
	case TSYNC:	SM_PROCEED(recv_mail);
			break;
	case SYN:	SM_PROCEED(recv_req);
			break;
	case ENQ:	SM_PROCEED(send_req);
			break;
	case 'C':
	case NAK:	PUTCHAR(EOT);
			SM_PROCEED(wait_command);
			break;
	case CAN:	SM_SUCCESS;  /* this is not in BT */
			break;
	default:	if (c > ' ') debug(10,"got '%c' waiting command",c);
			else debug(10,"got '\\%03o' waiting command",c);
			PUTCHAR(SUB);
			SM_PROCEED(wait_command);
			break;
	}

SM_STATE(recv_mail)

	if (recvfiles()) {SM_ERROR;}
	else {SM_PROCEED(wait_command);}

SM_STATE(send_req)

	if (sendbark()) {SM_ERROR;}
	else {SM_SUCCESS;}

SM_STATE(recv_req)

	if (recvbark()) {SM_ERROR;}
	else {SM_PROCEED(wait_command);}

SM_END
SM_RETURN


SM_DECL(rxftsc,"rxftsc")
SM_STATES
	recv_mail,send_mail,send_req,recv_req
SM_NAMES
	"recv_mail","send_mail","send_req","recv_req"
SM_EDECL

	int c,count=0;

SM_START(recv_mail)

SM_STATE(recv_mail)

	if (recvfiles()) {SM_ERROR;}
	else {SM_PROCEED(send_mail);}

SM_STATE(send_mail)

	if (count > 6) {SM_ERROR;}

	if (tosend == NULL) {SM_PROCEED(send_req);}

	PUTCHAR(TSYNC);
	c=GETCHAR(15);
	count++;
	if (c == TIMEOUT)
	{
		loginf("no NAK, cannot send mail");
		SM_ERROR;
	}
	else if (c < 0)
	{
		loginf("got error waiting for NAK: received %d",c);
		SM_ERROR;
	}
	else switch (c)
	{
	case 'C':
	case NAK:	if (xmsndfiles(tosend)) {SM_ERROR;}
			else {SM_PROCEED(send_req);}
			break;
	case CAN:	loginf("remote refused to pick mail");
			SM_SUCCESS;
			break;
	case EOT:	PUTCHAR(ACK);
			SM_PROCEED(send_mail);
			break;
	default:	if (c > ' ') debug(10,"got '%c' waiting NAK",c);
			else debug(10,"got '\\%03o' waiting NAK",c);
			SM_PROCEED(send_mail);
			break;
	}

SM_STATE(send_req)

	if (count > 6) {SM_ERROR;}

	if ( 1 /* has no .req file */ ) {SM_PROCEED(recv_req);}

	PUTCHAR(SYN);
	c=GETCHAR(15);
	count++;
	if (c == TIMEOUT)
	{
		loginf("no ENQ, cannot send requests");
		SM_ERROR;
	}
	else if (c < 0)
	{
		loginf("got error waiting for ENQ: received %d",c);
		SM_ERROR;
	}
	else switch (c)
	{
	case ENQ:	if (sendbark()) {SM_ERROR;}
			else {SM_PROCEED(recv_req);}
			break;
	case CAN:	loginf("remote refused to accept request");
			SM_PROCEED(recv_req);
			break;
	case 'C':
	case NAK:	PUTCHAR(EOT);
			SM_PROCEED(send_req);
			break;
	case SUB:	SM_PROCEED(send_req);
			break;
	default:	if (c > ' ') debug(10,"got '%c' waiting ENQ",c);
			else debug(10,"got '\\%03o' waiting ENQ",c);
			SM_PROCEED(send_req);
			break;
	}

SM_STATE(recv_req)

	if (recvbark()) {SM_ERROR;}
	else {SM_SUCCESS;}

SM_END
SM_RETURN


SM_DECL(recvfiles,"recvfiles")
SM_STATES
	recv_packet,scan_packet,recv_file
SM_NAMES
	"recv_packet","scan_packet","recv_file"
SM_EDECL

	int rc=0;
	char recvpktname[16];
	char *fpath;
	FILE *fp;
	faddr f,t;
	fa_list **tmpl;

SM_START(recv_packet)

SM_STATE(recv_packet)

	sprintf(recvpktname,"%08lx.pkt",(unsigned long)sequencer());
	rc=xmrecv(recvpktname);
	if (rc == 1) {SM_SUCCESS;}
	else if (rc == 0)
	{
		if (master) {SM_PROCEED(recv_file);}
		else {SM_PROCEED(scan_packet);}
	}
	else {SM_ERROR;}

SM_STATE(scan_packet)

	fpath=xstrcpy(inbound);
	fpath=xstrcat(fpath,"/");
	fpath=xstrcat(fpath,recvpktname);
	fp=fopen(fpath,"r");
	free(fpath);
	if (fp == NULL)
	{
		logerr("$cannot open received packet");
		SM_ERROR;
	}
	switch (getheader(&f,&t,fp))
	{
	case 3:	loginf("remote mistook us for %s",ascfnode(&t,0x1f));
		fclose(fp);
		SM_ERROR;
	case 0:	loginf("accepting session");
		fclose(fp);
		for (tmpl=&remote;*tmpl;tmpl=&((*tmpl)->next));
		(*tmpl)=(fa_list*)xmalloc(sizeof(fa_list));
		(*tmpl)->next=NULL;
		(*tmpl)->addr=(faddr*)xmalloc(sizeof(faddr));
		(*tmpl)->addr->zone=f.zone;
		(*tmpl)->addr->net=f.net;
		(*tmpl)->addr->node=f.node;
		(*tmpl)->addr->point=f.point;
		(*tmpl)->addr->name=NULL;
		(*tmpl)->addr->domain=NULL;
		for (tmpl=&remote;*tmpl;tmpl=&((*tmpl)->next))
			(void)nodelock((*tmpl)->addr);
			/* try lock all remotes, ignore locking result */
		if (((nlent=getnlent(remote->addr))) &&
		    (nlent->pflag != NL_DUMMY))
		{
			loginf("remote is a listed system");
			inbound=listinbound;
		}
		if (nlent) rdoptions(nlent);
		if (f.name)
		{
			loginf("remote gave correct password, protected FTS-0001 session");
			inbound=listinbound;
		}
		tosend=create_filelist(remote,ALL_MAIL,1);
		if (rc == 0) {SM_PROCEED(recv_file);}
		else {SM_SUCCESS;}
	default: loginf("received bad packet apparently from",ascfnode(&f,0x1f));
		fclose(fp);
		SM_ERROR;
	}

SM_STATE(recv_file)

	switch (xmrecv(NULL))
	{
	case 0:		SM_PROCEED(recv_file); break;
	case 1:		SM_SUCCESS; break;
	default:	SM_ERROR; break;
	}

SM_END
SM_RETURN

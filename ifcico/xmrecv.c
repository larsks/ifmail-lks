#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <utime.h>
#include "session.h"
#include "ttyio.h"
#include "statetbl.h"
#include "xutil.h"
#include "lutil.h"
#include "config.h"

#define XMBLKSIZ 128

extern unsigned INT16 crc16(char*,int);
extern unsigned char checksum(char*,int);
extern time_t sl2mtime(time_t);
extern time_t tl2mtime(time_t);
extern int m7recv(char*);

static int xm_recv(void);
static int resync(off_t);
FILE *openfile(char*,time_t,off_t,off_t*,int(*)(off_t));
int closefile(int);
static int closeit(int);
static char *recvname=NULL;
static char *fpath=NULL;
static FILE *fp=NULL;
static int last;
static time_t stm,etm;
static off_t startofs;
static long recv_blk;

int xmrecv(char*);
int xmrecv(name)
char *name;
{
	int rc;

	debug(11,"start xmodem receive \"%s\"",S(name));
	recvname=name;
	last=0;
	rc=xm_recv();
	if (fp) closeit(0);
	TTYWAIT(1);
	if (rc) return -1;
	else if (last) return 1;
	else return 0;
}

int closeit(success)
int success;
{
	off_t endofs;

	endofs=recv_blk*XMBLKSIZ;
	(void)time(&etm);
	if (etm == stm) etm++;
	loginf("%s %lu bytes in %lu seconds (%lu cps)",
		success?"received":"dropped after",
		(unsigned long)(endofs-startofs),etm-stm,
		(unsigned long)(endofs-startofs)/(etm-stm));
	fp=NULL;
	return closefile(success);
}

SM_DECL(xm_recv,"xmrecv")
SM_STATES
	sendnak0,waitblk0,
	sendnak,waitblk,recvblk,sendack,checktelink,
	recvm7,goteof
SM_NAMES
	"sendnak0","waitblk0",
	"sendnak","waitblk","recvblk","sendack","checktelink",
	"recvm7","goteof"
SM_EDECL

	int tmp;
	int crcmode=session_flags&FTSC_XMODEM_CRC;
	int count=0,junk=0,cancount=0;
	int header;
	struct _xmblk {
		unsigned char n1,n2;
		char data[XMBLKSIZ];
		unsigned char c1,c2;
	} xmblk;
	unsigned INT16 localcrc,remotecrc;
	unsigned char localcs,remotecs;
	long ackd_blk=-1L;
	long next_blk=1L;
	long last_blk=0L;
	off_t resofs;
	int telink=0;
	char tmpfname[16];
	off_t wsize;
	time_t remtime=0L;
	off_t remsize=0;
	char ctt[32];

	(void)time(&stm);
	recv_blk=-1L;

	if (recvname) strncpy(tmpfname,recvname,sizeof(tmpfname)-1);
	else tmpfname[0]='\0';

SM_START(sendnak0)

SM_STATE(sendnak0)

	if (count++ > 9)
	{
		loginf("too many errors while xmodem receive init");
		SM_ERROR;
	}
	if ((ackd_blk < 0) && crcmode && (count > 4))
	{
		debug(11,"no responce to 'C', try checksum mode");
		session_flags &= ~FTSC_XMODEM_CRC;
		crcmode=0;
	}

	if (crcmode) PUTCHAR('C');
	else PUTCHAR(NAK);

	junk=0;

	SM_PROCEED(waitblk0);

SM_STATE(waitblk0)

	header=GETCHAR(5);
	if (header == TIMEOUT)
	{
		debug(11,"timeout waiting for xmodem block 0 header, count=%d",
			count);
		if ((count > 2) && (session_flags & SESSION_IFNA))
		{
			loginf("timeout waiting for file in WaZOO session, report success");
			last=1;
			SM_SUCCESS;
		}
		SM_PROCEED(sendnak0);
	}
	else if (header < 0) {SM_ERROR;}
	else switch (header)
	{
	case EOT:	last=1;
			SM_SUCCESS;
			break;
	case CAN:	loginf("got CAN while xmodem receive init");
			SM_ERROR;
			break;
	case SOH:	SM_PROCEED(recvblk);
			break;
	case SYN:	telink=1;
			SM_PROCEED(recvblk);
			break;
	case ACK:	telink=1;
			SM_PROCEED(recvm7);
			break;
	case TSYNC:	SM_PROCEED(sendnak0);
			break;
	case NAK:
	case 'C':	PUTCHAR(EOT); /* other end still waiting us to send? */
			SM_PROCEED(waitblk0);
			break;
	default:	debug(11,"got '%s' waiting for block 0",
				printablec(header));
			if (junk++ > 300) {SM_PROCEED(sendnak0);}
			else {SM_PROCEED(waitblk0);}
			break;
	}

SM_STATE(sendnak)

	if (ackd_blk < 0) {SM_PROCEED(sendnak0);}

	if (count++ > 9)
	{
		loginf("too many errors while xmodem receive");
		SM_ERROR;
	}

	junk=0;

	if (remote_flags&FTSC_XMODEM_RES)
	{
		if (resync(ackd_blk*XMBLKSIZ)) {SM_ERROR;}
		else {SM_PROCEED(waitblk);}
	}
	else /* simple NAK */
	{
		debug(11,"negative acknowlege block %ld",ackd_blk+1);

		PUTCHAR(NAK);
		PUTCHAR(ackd_blk+1);
		PUTCHAR(~(ackd_blk+1));
		if (STATUS) {SM_ERROR;}
		else {SM_PROCEED(waitblk);}
	}

SM_STATE(sendack)

	ackd_blk=recv_blk;
	count=0;
	cancount=0;
	debug(11,"acknowlege block %ld",ackd_blk);

	PUTCHAR(ACK);
	PUTCHAR(ackd_blk);
	PUTCHAR(~ackd_blk);
	if (STATUS) {SM_ERROR;}
	SM_PROCEED(waitblk);

SM_STATE(waitblk)

	header=GETCHAR(15);
	if (header == TIMEOUT)
	{
		debug(11,"timeout waiting for xmodem block header, count=%d",
			count);
		SM_PROCEED(sendnak);
	}
	else if (header < 0) {SM_ERROR;}
	else switch (header)
	{
	case EOT:	if (last_blk && (ackd_blk != last_blk))
			{
				debug(11,"false EOT after %ld block, need after %ld",
					ackd_blk,last_blk);
				SM_PROCEED(waitblk);
			}
			else {SM_PROCEED(goteof);}
			break;
	case CAN:	if (cancount++ > 4)
			{
				closeit(0);
				loginf("got CAN while xmodem receive");
				SM_ERROR;
			}
			else {SM_PROCEED(waitblk);}
			break;
	case SOH:	SM_PROCEED(recvblk);
			break;
	default:	if (header < ' ') debug(11,"got '\\%03o' waiting SOH",
						header);
			else debug(11,"got '%c' waiting SOH",header);
			if (junk++ > 200) {SM_PROCEED(sendnak);}
			else {SM_PROCEED(waitblk);}
			break;
	}

SM_STATE(recvblk)

	GET((char*)&xmblk,(crcmode && (header != SYN))?
				sizeof(xmblk):
				sizeof(xmblk)-1,15);
	if (STATUS == STAT_TIMEOUT)
	{
		debug(11,"xmrecv timeout waiting for block body");
		SM_PROCEED(sendnak);
	}
	if (STATUS) {SM_ERROR;}
	if ((xmblk.n1&0xff) != ((~xmblk.n2)&0xff))
	{
		debug(11,"bad block number: 0x%02x/0x%02x (0x%02x)",
			xmblk.n1,xmblk.n2,(~xmblk.n2)&0xff);
		SM_PROCEED(waitblk);
	}
	recv_blk=ackd_blk-(ackd_blk-(unsigned)xmblk.n1);
	if (crcmode && (header != SYN))
	{
		remotecrc=(INT16)xmblk.c1<<8|xmblk.c2;
		localcrc=crc16(xmblk.data,sizeof(xmblk.data));
		if (remotecrc != localcrc)
		{
			debug(11,"bad crc: 0x%04x/0x%04x",remotecrc,localcrc);
			if (recv_blk == (ackd_blk+1)) {SM_PROCEED(sendnak);}
			else {SM_PROCEED(waitblk);}
		}
	}
	else
	{
		remotecs=xmblk.c1;
		localcs=checksum(xmblk.data,sizeof(xmblk.data));
		if (remotecs != localcs)
		{
			debug(11,"bad checksum: 0x%02x/0x%02x",remotecs,localcs);
			if (recv_blk == (ackd_blk+1)) {SM_PROCEED(sendnak);}
			else {SM_PROCEED(waitblk);}
		}
	}
	if ((ackd_blk == -1L) && (recv_blk == 0L)) {SM_PROCEED(checktelink);}
	if ((ackd_blk == -1L) && (recv_blk == 1L))
	{
		if (count < 3) {SM_PROCEED(sendnak0);}
		else ackd_blk=0L;
	}
	if (recv_blk < (ackd_blk+1L))
	{
		debug(11,"old block number %ld after %ld, go on",
			recv_blk,ackd_blk);
		SM_PROCEED(waitblk);
	}
	else if (recv_blk > (ackd_blk+1L))
	{
		debug(11,"bad block order: %ld after %ld, go on",
			recv_blk,ackd_blk);
		SM_PROCEED(waitblk);
	}

	debug(11,"received block %ld \"%s\"",
		recv_blk,printable(xmblk.data,128));

	if (fp == NULL)
	{
		if ((fp=openfile(tmpfname,remtime,remsize,&resofs,resync)) == NULL)
		{
			SM_ERROR;
		}
		else
		{
			if (resofs) ackd_blk=(resofs-1)/XMBLKSIZ+1L;
			else ackd_blk=-1L;
		}
		startofs=resofs;
		loginf("xmodem receive: \"%s\"",tmpfname);
	}
	
	if (recv_blk > next_blk)
	{
		logerr("xmrecv internal error: recv_blk %ld > next_blk %ld",
			recv_blk,next_blk);
		SM_ERROR;
	}
	if (recv_blk == next_blk)
	{
		if (recv_blk == last_blk) wsize=remsize%XMBLKSIZ;
		else wsize=XMBLKSIZ;
		if (wsize == 0) wsize=XMBLKSIZ;
		if ((tmp=fwrite(xmblk.data,wsize,1,fp)) != 1)
		{
			logerr("$error writing block %l (%d bytes) to file \"%s\" (fwrite return %d)",
				recv_blk,wsize,fpath,tmp);
			SM_ERROR;
		}
		else debug(11,"Block %ld size %d written (ret %d)",
			recv_blk,wsize,tmp);
		next_blk++;
	}
	else
	{
		debug(11,"recv_blk %ld < next_blk %ld, ack without writing",
			recv_blk,next_blk);
	}
	SM_PROCEED(sendack);

SM_STATE(checktelink)

	debug(11,"checktelink got \"%s\"",printable(xmblk.data,45));
	if (tmpfname[0] == '\0')
	{
		strncpy(tmpfname,xmblk.data+8,16);
	}
	else
	{
		loginf("Remote uses %s",printable(xmblk.data+25,-14));
		debug(11,"Remote file name \"%s\" discarded",
			printable(xmblk.data+8,-16));
	}
	remsize=((off_t)xmblk.data[0])+((off_t)xmblk.data[1]<<8)+
		((off_t)xmblk.data[2]<<16)+((off_t)xmblk.data[3]<<24);
	last_blk=(remsize-1)/XMBLKSIZ+1;
	if (header == SOH)
	{
		remtime=sl2mtime(((time_t)xmblk.data[4])+
			((time_t)xmblk.data[5]<<8)+
			((time_t)xmblk.data[6]<<16)+
			((time_t)xmblk.data[7]<<24));
		if (xmblk.data[40]) remote_flags |= FTSC_XMODEM_SLO;
		else remote_flags &= ~FTSC_XMODEM_SLO;
		if (xmblk.data[41]) remote_flags |= FTSC_XMODEM_RES;
		else remote_flags &= ~FTSC_XMODEM_RES;
		if (xmblk.data[42]) remote_flags |= FTSC_XMODEM_XOF;
		else remote_flags &= ~FTSC_XMODEM_XOF;
	}
	else /* Telink */
	{
		remtime=tl2mtime(((time_t)xmblk.data[4])+
			((time_t)xmblk.data[5]<<8)+
			((time_t)xmblk.data[6]<<16)+
			((time_t)xmblk.data[7]<<24));
/*
		if (xmblk.data[40]) remote_flags |= FTSC_XMODEM_NOACKS;
		else remote_flags &= ~FTSC_XMODEM_NOACKS;
*/
		if (xmblk.data[41]) session_flags |= FTSC_XMODEM_CRC;
		else session_flags &= ~FTSC_XMODEM_CRC;
	}
	debug(11,"%s block, session_flags=0x%04x, remote_flags=0x%04x",
		(header == SYN)?"Telink":"Sealink",session_flags,remote_flags);
	strcpy(ctt,date(remtime));
	debug(11,"Remote file size %lu timestamp %s",
		(unsigned long)remsize,ctt);

	if ((fp=openfile(tmpfname,remtime,remsize,&resofs,resync)) == NULL)
	{
		SM_ERROR;
	}
	if (resofs) ackd_blk=(resofs-1)/XMBLKSIZ+1L;
	else ackd_blk=-1L;
	startofs=resofs;

	loginf("xmodem receive: \"%s\" %ld bytes dated %s",
		tmpfname,remsize,ctt);

	if (ackd_blk == -1) {SM_PROCEED(sendack);}
	else {SM_PROCEED(waitblk);}

SM_STATE(recvm7)

	switch (m7recv(tmpfname))
	{
	case 0:	ackd_blk=0; SM_PROCEED(sendnak); break;
	case 1:	last=1; SM_SUCCESS; break;
	default: SM_PROCEED(sendnak);
	}

SM_STATE(goteof)

	closeit(1);
	if (ackd_blk == -1L) last=1;
	else
	{
		ackd_blk++;
		PUTCHAR(ACK);
		PUTCHAR(ackd_blk);
		PUTCHAR(~ackd_blk);
	}
	if (STATUS) {SM_ERROR;}
	SM_SUCCESS;

SM_END
SM_RETURN


int resync(resofs)
off_t resofs;
{
	char resynbuf[16];
	INT16 lcrc;
	int count=0;
	int gotack,gotnak;
	int c;
	long sblk;

	debug(11,"trying to resync at offset %ld",resofs);

	sblk=resofs/XMBLKSIZ+1;
	sprintf(resynbuf,"%ld",sblk);
	lcrc=crc16(resynbuf,strlen(resynbuf));
	gotack=0;
	gotnak=0;

	do
	{
		count++;
		PUTCHAR(SYN);
		PUTSTR(resynbuf);
		PUTCHAR(ETX);
		PUTCHAR(lcrc&0xff);
		PUTCHAR(lcrc>>8);
		do
		{
			if ((c=GETCHAR(5)) == ACK)
			{
				if ((c=GETCHAR(1)) == SOH) gotack=1;
				UNGETCHAR(c);
			}
			else if (c == NAK)
			{
				if ((c=GETCHAR(1)) == TIMEOUT) gotnak=1;
				UNGETCHAR(c);
			}
		}
		while (!gotack && !gotnak && (c >= 0));
		if ((c < 0) && (c != TIMEOUT)) return 1;
	}
	while (!gotack && !gotnak && (count < 6));

	if (gotack)
	{
		debug(11,"resyncing at offset %ld",resofs);
		return 0;
	}
	else
	{
		loginf("sealink resync at offset %ld failed",resofs);
		return 1;
	}
}

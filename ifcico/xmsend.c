#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include "session.h"
#include "lutil.h"
#include "ttyio.h"
#include "statetbl.h"
#include "version.h"

#define XMBLKSIZ 128
#define DEFAULT_WINDOW 127

extern unsigned INT16 crc16(char*,int);
extern unsigned char checksum(char*,int);
extern long atol(char*);
extern time_t mtime2sl(time_t);
extern int m7send(char*);

static char *ln,*rn;
static int flg;

static int xm_send(void);

int xmsend(char*,char*,int);
int xmsend(local,remote,fl)
char *local,*remote;
int fl;
{
	int rc;

	ln=local;
	rn=remote;
	flg=fl;
	rc=xm_send();
	TTYWAIT(1);
	if (rc) loginf("send failed");
	return rc;
}

SM_DECL(xm_send,"xmsend")
SM_STATES
	sendm7,sendblk0,waitack0,sendblk,writeblk,
	waitack,resync,sendeot
SM_NAMES
	"sendm7","sendblk0","waitack0","sendblk","writeblk",
	"waitack","resync","sendeot"
SM_EDECL

	FILE *fp;
	struct stat st;
	struct flock fl;
	unsigned INT16 lcrc=0,rcrc;
	int startstate;
	int crcmode,seamode,telink;
	int a,a1,a2;
	int i;
	time_t seatime;
	time_t stm,etm;

	unsigned char header=SOH;
	struct _xmblk {
		unsigned char n1;
		unsigned char n2;
		char data[XMBLKSIZ];
		unsigned char c1;
		unsigned char c2;
	} xmblk;
	int count=0;
	int cancount=0;
	int window;
	long last_blk;
	long send_blk;
	long next_blk;
	long ackd_blk;
	long tmp;

	char resynbuf[16];

	fl.l_type=F_RDLCK;
	fl.l_whence=0;
	fl.l_start=0L;
	fl.l_len=0L;

	(void)time(&stm);

	/* if we got 'C' than hopefully remote is sealink capable... */

	if (session_flags & FTSC_XMODEM_CRC)
	{
		telink=0;
		crcmode=1;
		session_flags |= FTSC_XMODEM_RES;
		session_flags |= FTSC_XMODEM_SLO;
		session_flags |= FTSC_XMODEM_XOF;
		window=DEFAULT_WINDOW;
		send_blk=0L;
		next_blk=0L;
		ackd_blk=-1L;
		startstate=sendblk0;
	}
	else 
	{
		telink=1;
		crcmode=0;
		session_flags &= ~FTSC_XMODEM_RES;
		session_flags |= FTSC_XMODEM_SLO;
		session_flags |= FTSC_XMODEM_XOF;
		window=1;
		send_blk=0L;
		next_blk=0L;
		ackd_blk=-1L;
		if (flg && !(session_flags & SESSION_IFNA))
			startstate=sendm7;
		else startstate=sendblk0;
	}

	seamode=-1; /* not yet sure about numbered ACKs */

	if (stat(ln,&st) != 0)
	{
		logerr("$cannot stat local file \"%s\" to send",S(ln));
		return 1;
	}
	last_blk=(st.st_size-1)/XMBLKSIZ+1;

	if ((fp=fopen(ln,"r")) == NULL)
	{
		logerr("$cannot open local file \"%s\" to send",S(ln));
		return 1;
	}
	fl.l_pid=getpid();
	if (fcntl(fileno(fp),F_SETLK,&fl) != 0)
	{
		loginf("$cannot lock local file \"%s\" to send, skip it",S(ln));
		return 0;
	}
	if (stat(ln,&st) != 0)
	{
		loginf("$cannot access local file \"%s\" to send, skip it",S(ln));
		return 0;
	}

	loginf("xmodem send \"%s\" as \"%s\", size=%lu",
		S(ln),S(rn),(unsigned long)st.st_size);

SM_START(startstate)

SM_STATE(sendm7)

	if (m7send(rn)) {SM_PROCEED(sendblk0);}
	else {SM_ERROR;}

SM_STATE(sendblk0)

	debug(11,"xmsendblk0 send:%ld, next:%ld, ackd:%ld, last:%ld",
		send_blk,next_blk,ackd_blk,last_blk);

	memset(xmblk.data,0,sizeof(xmblk.data));

	xmblk.data[0]=(st.st_size)&0xff;
	xmblk.data[1]=(st.st_size>>8)&0xff;
	xmblk.data[2]=(st.st_size>>16)&0xff;
	xmblk.data[3]=(st.st_size>>24)&0xff;
	seatime=mtime2sl(st.st_mtime);
	xmblk.data[4]=(seatime)&0xff;
	xmblk.data[5]=(seatime>>8)&0xff;
	xmblk.data[6]=(seatime>>16)&0xff;
	xmblk.data[7]=(seatime>>24)&0xff;
	strncpy(xmblk.data+8,rn,17);
	if (telink) 
		for (i=23;(i>8) && (xmblk.data[i] == '\0');i--)
			xmblk.data[i]=' ';
	sprintf(xmblk.data+25,"ifcico %s",version);
	xmblk.data[40]=((session_flags & FTSC_XMODEM_SLO) != 0);
	xmblk.data[41]=((session_flags & FTSC_XMODEM_RES) != 0);
	xmblk.data[42]=((session_flags & FTSC_XMODEM_XOF) != 0);

	debug(11,"sealink block: \"%s\"",printable(xmblk.data,44));

	next_blk=send_blk+1;
	SM_PROCEED(sendblk);

SM_STATE(sendblk)

	if (send_blk == 0) {SM_PROCEED(writeblk);}

	debug(11,"xmsendblk send:%ld, next:%ld, ackd:%ld, last:%ld",
		send_blk,next_blk,ackd_blk,last_blk);

	if (send_blk > last_blk)
	{
		if (send_blk == (last_blk+1)) {SM_PROCEED(sendeot);}
		else if (ackd_blk < last_blk) {SM_PROCEED(waitack);}
		else
		{
			(void)time(&etm);
			if (etm == stm) etm++;
			loginf("sent %lu bytes in %lu seconds (%lu cps)",
				(unsigned long)st.st_size,etm-stm,
				(unsigned long)st.st_size/(etm-stm));
			fclose(fp);
			SM_SUCCESS;
		}
	}

	memset(xmblk.data,SUB,sizeof(xmblk.data));

	if (send_blk != next_blk)
	if (fseek(fp,(send_blk-1)*XMBLKSIZ,SEEK_SET) != 0)
	{
		logerr("$fseek error setting block %ld (byte %lu) in file \"%s\"",
			send_blk,(send_blk-1)*XMBLKSIZ,S(ln));
		SM_ERROR;
	}
	if (fread(xmblk.data,1,XMBLKSIZ,fp) <= 0)
	{
		logerr("$read error for block %lu in file \"%s\"",
			send_blk,S(ln));
		SM_ERROR;
	}
	next_blk=send_blk+1;

	SM_PROCEED(writeblk);

SM_STATE(writeblk)

	xmblk.n1=send_blk&0xff;
	xmblk.n2=~xmblk.n1;
	if (crcmode)
	{
		lcrc=crc16(xmblk.data,sizeof(xmblk.data));
		xmblk.c1=(lcrc>>8)&0xff;
		xmblk.c2=lcrc&0xff;
	}
	else
	{
		xmblk.c1=checksum(xmblk.data,sizeof(xmblk.data));
	}
	
	PUTCHAR(header);
	PUT((char*)&xmblk,crcmode?sizeof(xmblk):sizeof(xmblk)-1);
	if (STATUS) {SM_ERROR;}
	if (crcmode)
		debug(11,"sent '\\%03o',no 0x%02x %d bytes crc 0x%04x",
			header,xmblk.n1,XMBLKSIZ,lcrc);
	else
		debug(11,"sent '\\%03o',no 0x%02x, %d bytes checksum 0x%02x",
			header,xmblk.n1,XMBLKSIZ,xmblk.c1);
	send_blk++;
	SM_PROCEED(waitack);

SM_STATE(waitack)

	if ((count > 4) && (ackd_blk < 0))
	{
		loginf("cannot send sealink block, try xmodem");
		window=1;
		ackd_blk++;
		SM_PROCEED(sendblk);
	}
	if (count > 9)
	{
		loginf("too many errors in xmodem send");
		SM_ERROR;
	}

	if ((ackd_blk < 0) || 
	    (send_blk > (last_blk+1)) ||
	    ((send_blk-ackd_blk) > window))
		TTYWAIT(1);
	else TTYWAIT(0);

	a=GETCHAR(20);
	if (a == EMPTY) {SM_PROCEED(sendblk);}

	TTYWAIT(1);
	if (a == TIMEOUT)
	{
		if(count++ > 9)
		{
			loginf("too many tries to send block");
			SM_ERROR;
		}
		debug(11,"timeout waiting for ACK");
		send_blk=ackd_blk+1;
		SM_PROCEED(sendblk);
	}
	else if (a < 0)
	{
		SM_ERROR;
	}
	else switch (a)
	{
	case ACK:
		count=0;
		cancount=0;
		switch (seamode)
		{
		case -1: if ((a1=GETCHAR(1)) < 0)
			{
				seamode=0;
				UNGETCHAR(a);
				SM_PROCEED(waitack);
			}
			else if ((a2=GETCHAR(1)) < 0)
			{
				seamode=0;
				UNGETCHAR(a1);
				UNGETCHAR(a);
				SM_PROCEED(waitack);
			}
			else if ((a1&0xff) != ((~a2)&0xff))
			{
				seamode=0;
				UNGETCHAR(a2);
				UNGETCHAR(a1);
				UNGETCHAR(a);
				SM_PROCEED(waitack);
			}
			else
			{
				seamode=1;
				UNGETCHAR(a2);
				UNGETCHAR(a1);
				UNGETCHAR(a);
				SM_PROCEED(waitack);
			}
			break;
		case 0:
			ackd_blk++;
			SM_PROCEED(sendblk);
			break;
		case 1:
			a1=GETCHAR(1);
			a2=GETCHAR(1);
			if ((a1 < 0) || (a2 < 0) || (a1 != ((~a2)&0xff)))
			{
				debug(11,"bad ACK: 0x%02x/0x%02x, ignore",
					a1,a2);
				SM_PROCEED(sendblk);
			}
			else
			{
				if (a1 != ((ackd_blk+1)&0xff))
					debug(11,"got ACK %d, expected %d",
						a1,(ackd_blk+1)&0xff);
				tmp=send_blk-((send_blk-a1)&0xff);
				if ((tmp > ackd_blk) && (tmp < send_blk))
					ackd_blk=tmp;
				else
					debug(11,"bad ACK: %ld, ignore",
						a1,a2);
				if ((ackd_blk+1) == send_blk)
					{SM_PROCEED(sendblk);}
				else /* read them all if more than 1 */
					{SM_PROCEED(waitack);}
			}
			break;
		}
		break;
	case NAK:	if (ackd_blk <= 0) crcmode=0;
			count++;
			send_blk=ackd_blk+1;
			SM_PROCEED(sendblk);
			break;
	case SYN:	SM_PROCEED(resync);
			break;
	case DC3:	if (session_flags & FTSC_XMODEM_XOF)
			while (((a=GETCHAR(15)) > 0) && (a != DC1))
				if (a < 0) debug(11,"got %d waiting for DC1",a);
				else debug(11,"got '%s' waiting for DC1",
					printablec(a));
			SM_PROCEED(waitack);
			break;
	case CAN:	if (cancount++ > 5)
			{
				loginf("remote requested cancel transfer");
				SM_ERROR;
			}
			else {SM_PROCEED(waitack);}
			break;
	case 'C':	if (ackd_blk < 0)
			{
				crcmode=1;
				count++;
				send_blk=ackd_blk+1;
				SM_PROCEED(sendblk);
			}
			/* fallthru */
	default:	if (a < ' ') debug(11,"got '\\%03o' waiting for ACK",a);
			else debug(11,"got '%c' waiting for ACK",a);
			SM_PROCEED(waitack);
			break;
	}

SM_STATE(resync)

	if (count++ > 9)
	{
		loginf("too may tries to resync");
		SM_ERROR;
	}

	i=-1;
	do
	{
		a=GETCHAR(15);
		resynbuf[++i]=a;
	}
	while ((a >= '0') && (a <= '9') && (i < sizeof(resynbuf)-1));
	resynbuf[i]='\0';
	debug(11,"got resync \"%s\", i=%d",resynbuf,i);
	lcrc=crc16(resynbuf,strlen(resynbuf));
	rcrc=0;
	if (a != ETX)
	{
		if (a > 0) loginf("got %d waiting for resync",a);
		else loginf("got %s waiting for resync",printablec(a));
		PUTCHAR(NAK);
		SM_PROCEED(waitack);
	}
	if ((a=GETCHAR(1)) < 0)
	{
		PUTCHAR(NAK);
		SM_PROCEED(waitack);
	}
	rcrc=a&0xff;
	if ((a=GETCHAR(1)) < 0)
	{
		PUTCHAR(NAK);
		SM_PROCEED(waitack);
	}
	rcrc |= (a << 8);
	if (rcrc != lcrc)
	{
		loginf("bad resync crc: 0x%04x != 0x%04x",lcrc,rcrc);
		PUTCHAR(NAK);
		SM_PROCEED(waitack);
	}
	send_blk=atol(resynbuf);
	ackd_blk=send_blk-1;
	loginf("resyncing at block %ld (byte %lu)",
		send_blk,(send_blk-1L)*XMBLKSIZ);
	PUTCHAR(ACK);
	SM_PROCEED(sendblk);

SM_STATE(sendeot)

	PUTCHAR(EOT);
	if (STATUS) {SM_ERROR;}
	send_blk++;
	SM_PROCEED(waitack);

SM_END
SM_RETURN


int xmsndfiles(file_list*);
int xmsndfiles(tosend)
file_list *tosend;
{
	int rc,c,gotnak,count;
	file_list *nextsend;

	for (nextsend=tosend;nextsend;nextsend=nextsend->next)
	if (*(nextsend->local) != '~')
	{	
		if (nextsend->remote)
		if ((rc=xmsend(nextsend->local,nextsend->remote,
				(nextsend != tosend)))) /* send m7 for rest */
			return rc; /* and thus avoid execute_disposition() */
		else 
		{
			gotnak=0;
			count=0;
			while (!gotnak && (count < 6))
			{
				c=GETCHAR(15);
				if (c < 0) return STATUS;
				if (c == CAN)
				{
					loginf("remote refused receiving");
					return 1;
				}
				if ((c == 'C') || (c == NAK)) gotnak=1;
				else debug(11,"got '%s' waiting NAK",
					printablec(c));
			}
			if (c == 'C') session_flags |= FTSC_XMODEM_CRC;
			if (!gotnak) return 1;
		}
		execute_disposition(nextsend);
	}
	PUTCHAR(EOT);
	return STATUS;
}

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "ttyio.h"
#include "xutil.h"
#include "lutil.h"
#include "session.h"
#include "zmodem.h"
#include "emsi.h"

static int initsend(void);
static int sendfile(char*,char*);
static int finsend(void);

static int getzrxinit(void);
static int sendzsinit(void);
static int zfilbuf(void);
static int zsendfile(char*,int);
static int zsendfdata(void);
static int getinsync(int);

static FILE *in;
static int Test=0;
static int Eofseen;
static int Rxflags;
static int Usevhdrs;
static int Wantfcs32=TRUE;
static int Rxbuflen;
static int Txwindow;
static int Txwcnt;
static int blklen;
static int blkopt;
static int Txwspac;
static int errors;
static int Lastsync;
static int bytcnt;
static int Lrxpos=0;
static int Lztrans=0;
static int Lzmanag=0;
static int Lskipnocor=0;
static int Lzconv=0;
static int Beenhereb4;
static char Myattn[]={0};
static long startime,endtime;
static long skipsize;
#if 1
static char *qbf=
 "The quick brown fox jumped over the lazy dog's back 1234567890\r\n";
#else
static char *qbf=
 "В чащах юга жил бы цитрус? Да! Но фальшивый экземпляр 0123456789\r\n";
#endif

extern void get_frame_buffer(void);

int zmsndfiles(file_list*);
int zmsndfiles(lst)
file_list *lst;
{
	int rc,maxrc=0;
	file_list *tmpf;

	loginf("start %s send%s",
		(emsi_local_protos & ZAP)?"ZedZap":"Zmodem",
		lst?"":" (dummy)");

	get_frame_buffer();

	if ((rc=initsend())) return abs(rc);
	for (tmpf=lst;tmpf && (maxrc < 2);tmpf=tmpf->next)
	{
		if (tmpf->remote)
		{
			rc=sendfile(tmpf->local,tmpf->remote);
			rc=abs(rc);
			if (rc > maxrc) maxrc=rc;
			if (rc == 0) execute_disposition(tmpf);
		}
		else if (maxrc == 0) execute_disposition(tmpf);
	}
	if (maxrc < 2)
	{
		rc=finsend();
		rc=abs(rc);
	}
	if (rc > maxrc) maxrc=rc;

	loginf("zmodem send rc=%d",maxrc);
	return (maxrc<2)?0:maxrc;
}

static int initsend(void)
{
	debug(11,"initsend");

	PUTSTR("rz\r");
	stohdr(0L);
	zshhdr(4,ZRQINIT,Txhdr);
	if (getzrxinit())
	{
		loginf("unable to initiate zmodem send");
		return 1;
	}
	return 0;
}

static int finsend(void)
{
	int i,rc;

	debug(11,"finsend");
	while (GETCHAR(1) >= 0) /*nothing*/;
	for (i=0;i<30;i++)
	{
		stohdr(0L);
		zshhdr(4,ZFIN,Txhdr);
		if ((rc=zgethdr(Rxhdr,0)) == ZFIN) PUTSTR("OO");
		if ((rc == ZFIN) || (rc == ZCAN) || (rc < 0)) break;
	}
	return (rc != ZFIN);
}

static int sendfile(ln,rn)
char *ln,*rn;
{
	int rc=0;
	struct stat st;
	struct flock fl;
	int bufl;
	int sverr;

	fl.l_type=F_RDLCK;
	fl.l_whence=0;
	fl.l_start=0L;
	fl.l_len=0L;
	if (txbuf == NULL) txbuf=xmalloc(MAXBLOCK);

	skipsize=0L;
	if ((in=fopen(ln,"r")) == NULL)
	{
		sverr=errno;
		logerr("$zmsend cannot open file %s, skipping",S(ln));
		if (sverr == ENOENT) return 0;
		else return 1;
	}
	if (fcntl(fileno(in),F_SETLK,&fl) != 0)
	{
		loginf("$zmsend cannot lock file %s, skipping",S(ln));
		fclose(in);
		return 1;
	}
	if (stat(ln,&st) != 0)
	{
		loginf("$cannot access \"%s\", skipping",S(ln));
		fclose(in);
		return 1;
	}

	loginf("zmodem send \"%s\" as \"%s\" (%lu bytes)",
		S(ln),S(rn),(unsigned long)st.st_size);
	(void)time(&startime);

	sprintf(txbuf,"%s %lu %lo %o 0 0 0",
		rn,(unsigned long)st.st_size,
		st.st_mtime+(st.st_mtime%2),
		st.st_mode);
	bufl=strlen(txbuf);
	*(strchr(txbuf,' '))='\0'; /*hope no blanks in filename*/

	Eofseen = 0;
	rc=zsendfile(txbuf,bufl);
	if (rc == ZSKIP)
	{
		loginf("file %s considered normally sent",S(ln));
		return 0;
	}
	else if (rc == OK)
	{
		(void)time(&endtime);
		if ((startime=endtime-startime) == 0) startime=1;
		loginf("sent %lu bytes in %ld seconds (%ld cps)",
			(unsigned long)st.st_size-skipsize,startime,
			(long)(st.st_size-skipsize)/startime);
		return 0;
	}
	else return 2;
}

/*
 * Get the receiver's init parameters
 */
int getzrxinit(void)
{
	register n;

	debug(11,"getzrxinit");
	for (n=10; --n>=0; ) {
		
		switch (zgethdr(Rxhdr, 1)) {
		case ZCHALLENGE:	/* Echo receiver's challenge numbr */
			stohdr(Rxpos);
			zshhdr(4, ZACK, Txhdr);
			continue;
		case ZCOMMAND:		/* They didn't see out ZRQINIT */
			stohdr(0L);
			zshhdr(4, ZRQINIT, Txhdr);
			continue;
		case ZRINIT:
			Rxflags = 0377 & Rxhdr[ZF0];
			Usevhdrs = Rxhdr[ZF1] & CANVHDR;
			Txfcs32 = (Wantfcs32 && (Rxflags & CANFC32));
			Zctlesc |= Rxflags & TESCCTL;
			Rxbuflen = (0377 & Rxhdr[ZP0])+((0377 & Rxhdr[ZP1])<<8);
			if ( !(Rxflags & CANFDX))
				Txwindow = 0;
			debug(11,"remote allowed Rxbuflen=%d", Rxbuflen);

			/* Set initial subpacket length */
			if (blklen < 1024) {	/* Command line override? */
				blklen = 1024;
			}
			if (Rxbuflen && blklen>Rxbuflen)
				blklen = Rxbuflen;
			if (blkopt && blklen > blkopt)
				blklen = blkopt;
			/* if (Rxbuflen == 0) Rxbuflen=MAXBLOCK; */
			/* leave it off for now, because of problems reported */
			debug(11,"Rxbuflen=%d blklen=%d", Rxbuflen, blklen);
			debug(11,"Txwindow = %u Txwspac = %d", Txwindow, Txwspac);


			if (Lztrans == ZTRLE && (Rxflags & CANRLE))
				Txfcs32 = 2;
			else
				Lztrans = 0;

			return (sendzsinit());
		case ZCAN:
		case ERROR:
			return ERROR;
		case HANGUP:
			return HANGUP;
		case TIMEOUT:
			stohdr(0L);
			zshhdr(4, ZRQINIT, Txhdr);
			continue;
		case ZRQINIT:
			if (Rxhdr[ZF0] == ZCOMMAND)
				continue;
		default:
			zshhdr(4, ZNAK, Txhdr);
			continue;
		}
	}
	return ERROR;
}

/* Send send-init information */
int sendzsinit(void)
{
	register c;

	debug(11,"sendzsinit");
	if (Myattn[0] == '\0' && (!Zctlesc || (Rxflags & TESCCTL & ~CANRLE)))
		return OK;
	errors = 0;
	for (;;) {
		stohdr(0L);
		Txhdr[ZF0] = Txhdr[ZF0] & ~CANRLE ;
		if (Zctlesc) {
			Txhdr[ZF0] |= TESCCTL; zshhdr(4, ZSINIT, Txhdr);
		}
		else
			zsbhdr(4, ZSINIT, Txhdr);
		zsdata(Myattn, ZATTNLEN, ZCRCW);
		c = zgethdr(Rxhdr, 1);
		switch (c) {
		case ERROR:
		case ZCAN:
			return ERROR;
		case HANGUP:
			return HANGUP;
		case ZACK:
			return OK;
		default:
			if (++errors > 19)
				return ERROR;
			continue;
		}
	}
}

/* Fill buffer with blklen chars */
int zfilbuf(void)
{
	int n;

	debug(11,"zfilbuf (%d)",blklen);
	n = fread(txbuf, 1, blklen, in);
	if (n < blklen)
		Eofseen = 1;
	debug(11,"zfilbuf return %d",n);
	return n;
}

/* Send file name and related info */
int zsendfile(buf, blen)
char *buf;
int blen;
{
	register c;
	register INT32 crc=-1L;
	long lastcrcrq = -1;

	debug(11,"zsendfile %s (%d)",buf,blen);
	for (errors=0; ++errors<11;) {
		Txhdr[ZF0] = Lzconv;	/* file conversion request */
		Txhdr[ZF1] = Lzmanag;	/* file management request */
		if (Lskipnocor)
			Txhdr[ZF1] |= ZMSKNOLOC;
		Txhdr[ZF2] = Lztrans;	/* file transport request */
		Txhdr[ZF3] = 0;
		zsbhdr(4, ZFILE, Txhdr);
		zsdata(buf, blen, ZCRCW);
again:
		c = zgethdr(Rxhdr, 1);
		switch (c) {
		case ZRINIT:
			while ((c = GETCHAR(5)) > 0)
				if (c == ZPAD) {
					goto again;
				}
			continue;
		case ZCAN:
		case ERROR:
		case HANGUP:
		case TIMEOUT:
		case ZABORT:
		case ZFIN:
			loginf("Got %s on pathname", frametypes[c+FTOFFSET]);
			return c;
		default:
			loginf("Got %d frame type on pathname", c);
			continue;
		case ZNAK:
			continue;
		case ZCRC:
			if (Rxpos != lastcrcrq) {
				lastcrcrq = Rxpos;
				crc = 0xFFFFFFFFL;
				fseek(in, 0L, 0);
				while (((c = getc(in)) != EOF) && --lastcrcrq)
					crc = updcrc32(c, crc);
				crc = ~crc;
				clearerr(in);	/* Clear possible EOF */
				lastcrcrq = Rxpos;
			}
			stohdr(crc);
			zsbhdr(4, ZCRC, Txhdr);
			goto again;
		case ZFERR:
		case ZSKIP:
			loginf("File skipped by receiver request");
			fclose(in); return c;
		case ZRPOS:
			/*
			 * Suppress zcrcw request otherwise triggered by
			 * lastyunc==bytcnt
			 */
			if (Rxpos > 0) skipsize=Rxpos;
			if (fseek(in, Rxpos, 0))
				return ERROR;
			Lastsync = (bytcnt = Txpos = Lrxpos = Rxpos) -1;
			return zsendfdata();
		}
	}
	fclose(in); return ERROR;
}

/* Send the data in the file */
int zsendfdata(void)
{
	register c=0, e, n;
	register newcnt;
	register long tcount = 0;
	int junkcount;		/* Counts garbage chars received by TX */
	static int tleft = 6;	/* Counter for test mode */
	int maxblklen,goodblks=0,goodneeded=8;

	debug(11,"zsendfdata");

	/* XXX strnge problem with 8K blocks - cut down   XXX */
	if (emsi_local_protos & ZAP) maxblklen=MAXBLOCK/2;
	else maxblklen=1024;
	if (Rxbuflen && (maxblklen > Rxbuflen)) maxblklen=Rxbuflen;

	junkcount = 0;
	Beenhereb4 = 0;
somemore:

	if (0) {

waitack:
		junkcount = 0;
		c = getinsync(0);
gotack:
		switch (c) {
		default:
		case ZCAN:
			fclose(in);
			return ERROR;
		case ZSKIP:
			fclose(in);
			return c;
		case ZACK:
			break;
		case ZRPOS:
			blklen=((blklen >> 2) > 64) ? (blklen >> 2) : 64;
			goodblks=0;
			goodneeded=((goodneeded<<1) > 16) ? 16 :
				goodneeded << 1;
			break;
		case ZRINIT:
			fclose(in);
			return OK;
		case TIMEOUT:
			goto to;
		}
		/*
		 * If the reverse channel can be tested for data,
		 *  this logic may be used to detect error packets
		 *  sent by the receiver, in place of setjmp/longjmp
		 *  rdchk(fd) returns non 0 if a character is available
		 */
		TTYWAIT(0);
		if ((c=GETCHAR(1)) == EMPTY)
		{
			TTYWAIT(1);
		}
		else if (c < 0)
		{
			return c;
		}
		else switch (c)
		{
		case CAN:
		case ZPAD:
			TTYWAIT(1);
			c = getinsync(1);
			goto gotack;
		case DC3:		/* Wait a while for an XON */
		case DC3|0200:
			TTYWAIT(1);
			GETCHAR(10);
		}

	}
to:
	newcnt = Rxbuflen;
	Txwcnt = 0;
	stohdr(Txpos);
	zsbhdr(4, ZDATA, Txhdr);

	/*
	 * Special testing mode.  This should force receiver to Attn,ZRPOS
	 *  many times.  Each time the signal should be caught, causing the
	 *  file to be started over from the beginning.
	 */
	if (Test) {
		if ( --tleft)
			while (tcount < 20000) {
				printf(qbf); fflush(stdout);
				tcount += strlen(qbf);
				TTYWAIT(0);
				if ((c=GETCHAR(1)) == EMPTY)
				{
					TTYWAIT(1); break;
				}
				else if (c < 0)
				{
					return c;
				}
				else switch (c)
				{
				case CAN:
				case ZPAD:
					TTYWAIT(1);
					goto waitack;
				case DC3:	/* Wait for XON */
				case DC3|0200:
					TTYWAIT(1);
					GETCHAR(10);
				}
			}
		sleep(3); fflush(stdout);
		printf("\nsz: Tcount = %ld\n", tcount);
		if (tleft) {
			printf("ERROR: Interrupts Not Caught\n");
			exit(1);
		}
		exit(0);
	}

	do {
		n = zfilbuf();
		if (Eofseen)
			e = ZCRCE;
		else if (junkcount > 3)
			e = ZCRCW;
		else if (bytcnt == Lastsync)
			e = ZCRCW;
		else if (Rxbuflen && (newcnt -= n) <= 0)
			e = ZCRCW;
		else if (Txwindow && (Txwcnt += n) >= Txwspac) {
			Txwcnt = 0;  e = ZCRCQ;
		} else
			e = ZCRCG;
		debug(11,"%7ld ZMODEM%s    ",
			  Txpos, Crc32t?" CRC-32":"");
		zsdata(txbuf, n, e);
		bytcnt = Txpos += n;

		if ((blklen < maxblklen) && (++goodblks > goodneeded))
		{
			blklen = ((blklen << 1) < maxblklen) ? blklen << 1 :
				maxblklen;
			goodblks = 0;
		}

		if (e == ZCRCW)
			goto waitack;
		/*
		 * If the reverse channel can be tested for data,
		 *  this logic may be used to detect error packets
		 *  sent by the receiver, in place of setjmp/longjmp
		 *  rdchk(fd) returns non 0 if a character is available
		 */
		TTYWAIT(0);
		if ((c=GETCHAR(1)) == EMPTY)
		{
			TTYWAIT(1);
		}
		else if (c < 0)
		{
			return c;
		}
		else switch (c)
		{
		case CAN:
		case ZPAD:
			TTYWAIT(1);
			c = getinsync(1);
			if (c == ZACK)
				break;
			/* zcrce - dinna wanna starta ping-pong game */
			zsdata(txbuf, 0, ZCRCE);
			goto gotack;
		case DC3:		/* Wait a while for an XON */
		case DC3|0200:
			TTYWAIT(1);
			GETCHAR(10);
		default:
			++junkcount;
		}
		if (Txwindow) {
			while ((tcount = (Txpos - Lrxpos)) >= Txwindow) {
				debug(11,"%ld window >= %u", tcount, Txwindow);
				if (e != ZCRCQ)
					zsdata(txbuf, 0, e = ZCRCQ);
				c = getinsync(1);
				if (c != ZACK) {
					zsdata(txbuf, 0, ZCRCE);
					goto gotack;
				}
			}
			debug(11,"window = %ld", tcount);
		}
	} while (!Eofseen);

	for (;;) {
		stohdr(Txpos);
		zsbhdr(4, ZEOF, Txhdr);
		switch (getinsync(0)) {
		case ZACK:
			continue;
		case ZRPOS:
			goto somemore;
		case ZRINIT:
			fclose(in);
			return OK;
		case ZSKIP:
			fclose(in);
			loginf("File skipped by receiver request");
			return c;
		default:
			debug(11,"Got %d trying to send end of file", c);
			fclose(in);
			return ERROR;
		}
	}
}

/*
 * Respond to receiver's complaint, get back in sync with receiver
 */
int getinsync(flag)
int flag;
{
	register c;

	debug(11,"getinsync");
	for (;;) {
		c = zgethdr(Rxhdr, 0);
		switch (c) {
		case HANGUP:
			return HANGUP;
		case ZCAN:
		case ZABORT:
		case ZFIN:
		case ERROR:
		case TIMEOUT:
			loginf("Got %s sending data", frametypes[c+FTOFFSET]);
			blklen=64;
			/* return TIMEOUT; Why timeout??? */
			return c;
		case ZRPOS:
			/* ************************************* */
			/*  If sending to a buffered modem, you  */
			/*   might send a break at this point to */
			/*   dump the modem's buffer.		 */
			clearerr(in);	/* In case file EOF seen */
			if (fseek(in, Rxpos, 0))
				return ERROR;
			Eofseen = 0;
			bytcnt = Lrxpos = Txpos = Rxpos;
			if (Lastsync == Rxpos) {
				if (++Beenhereb4 > 12) {
					loginf("Can't send block");
					return ERROR;
				}
				/* if (Beenhereb4 > 4)
					if (blklen > 32)
						blklen /= 2; */
			}
			else Beenhereb4=0;
			Lastsync = Rxpos;
			return c;
		case ZACK:
			Lrxpos = Rxpos;
			if (flag || Txpos == Rxpos)
				return ZACK;
			continue;
		case ZRINIT:
			return c;
		case ZSKIP:
			loginf("File skipped by receiver request");
			return c;
		default:
			zsbhdr(4, ZNAK, Txhdr);
			continue;
		}
	}
}

#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <utime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/stat.h>
#if defined(HAS_STATFS)
#if defined(STATFS_IN_VFS_H)
#include <sys/vfs.h>
#elif defined(STATFS_IN_STATFS_H)
#include <sys/statfs.h>
#elif defined(STATFS_IN_STATVFS_H)
#include <sys/statvfs.h>
#elif defined(STATFS_IN_MOUNT_H)
#include <sys/param.h>
#include <sys/mount.h>
#else
#error No include for statfs() call defined
#endif
#elif defined(HAS_STATVFS)
#include <sys/statvfs.h>
#endif
#include "xutil.h"
#include "lutil.h"
#include "ttyio.h"
#include "zmodem.h"
#include "session.h"
#include "config.h"
#include "emsi.h"

#ifndef BELEIVE_ZFIN
#define BELEIVE_ZFIN 2
#endif

static FILE *fout=NULL;

static int Usevhdrs;
static long rxbytes;
static int Eofseen;		/* indicates cpm eof (^Z) has been received */
static int errors;
static time_t startime,etime;
static long sbytes;

#define DEFBYTL 2000000000L	/* default rx file size */
static long Bytesleft;		/* number of bytes of incoming file left */
static long Modtime;		/* Unix style mod time for incoming file */
static int Filemode;		/* Unix style mode for incoming file */
#ifndef PATH_MAX
#define PATH_MAX 512
#endif

static int Thisbinary;		/* current file is to be received in bin mode */

static char *secbuf=0;

static int tryzhdrtype;
static char zconv;		/* ZMODEM file conversion request */
static char zmanag;		/* ZMODEM file management request */
static char ztrans;		/* ZMODEM file transport request */

static int resync(off_t);
static int tryz(void);
static int rzfiles(void);
static int rzfile(void);
static void zmputs(char*);
int closefile(int);
int closeit(int);
FILE *openfile(char*,time_t,off_t,off_t*,int(*)(off_t));
static int putsec(char*,int);
static int procheader(char*);
static int ackbibi(void);
#if defined(HAS_STATFS) | defined(HAS_STATVFS)
static long getfree(void);
#endif

void get_frame_buffer(void);

int zmrcvfiles(void);
int zmrcvfiles(void)
{
	int rc;

	loginf("start %s receive",
		(emsi_local_protos & ZAP)?"ZedZap":"Zmodem");

	get_frame_buffer();

	if (secbuf == NULL) secbuf=xmalloc(MAXBLOCK+1);
	tryzhdrtype=ZRINIT;
	if ((rc=tryz()) < 0)
	{
		loginf("zmrecv could not initiate receive, rc=%d",rc);
	}
	else switch (rc)
	{
	case ZCOMPL:	rc=0; break;
	case ZFILE:	rc=rzfiles(); break;
	}
	if (fout)
	{ 
		if (closeit(0)) {
			logerr("Error closing file");
		}
	}	
	loginf("zmodem receive rc=%d",rc);
	return abs(rc);
}

/*
 * Initialize for Zmodem receive attempt, try to activate Zmodem sender
 *  Handles ZSINIT frame
 *  Return ZFILE if Zmodem filename received, -1 on error,
 *   ZCOMPL if transaction finished,  else 0
 */
int tryz(void)
{
	register c, n, numfin=0;
	register cmdzack1flg;

	debug(11,"tryz");
	for (n=15; --n>=0; ) {
		/* Set buffer length (0) and capability flags */
		stohdr(0L);
		Txhdr[ZF0] = CANFC32|CANFDX|CANOVIO;
		if (Zctlesc)
			Txhdr[ZF0] |= TESCCTL;

/* RJM 1999-07-07: This has been commented out because ifcico may think
 * it can do RLE, but it's blatantly not working here.
 */
#if 0
		Txhdr[ZF0] |= CANRLE;
#endif
		Txhdr[ZF1] = CANVHDR;
		zshhdr(4,tryzhdrtype, Txhdr);
		if (tryzhdrtype == ZSKIP)       /* Don't skip too far */
			tryzhdrtype = ZRINIT;   /* CAF 8-21-87 */

again:
		switch (zgethdr(Rxhdr, 0)) {
		case ZRQINIT:
			if (Rxhdr[ZF3] & 0x80)
				Usevhdrs = 1;	/* we can var header */
			continue;
		case ZEOF:
			continue;
		case TIMEOUT:
			if (numfin > 0) return ackbibi();
			else continue;
		case ZFILE:
			zconv = Rxhdr[ZF0];
			zmanag = Rxhdr[ZF1];
			ztrans = Rxhdr[ZF2];
			if (Rxhdr[ZF3] & ZCANVHDR)
				Usevhdrs = TRUE;
			tryzhdrtype = ZRINIT;
			c = zrdata(secbuf, MAXBLOCK);
			if (c == GOTCRCW)
				return ZFILE;
			zshhdr(4,ZNAK, Txhdr);
			goto again;
		case ZSINIT:
			Zctlesc = TESCCTL & Rxhdr[ZF0];
			if (zrdata(Attn, ZATTNLEN) == GOTCRCW) {
				stohdr(1L);
				zshhdr(4,ZACK, Txhdr);
				goto again;
			}
			zshhdr(4,ZNAK, Txhdr);
			goto again;
		case ZFREECNT:
			stohdr(getfree());
			zshhdr(4,ZACK, Txhdr);
			goto again;
		case ZCOMMAND:
			cmdzack1flg = Rxhdr[ZF0];
			if (zrdata(secbuf, MAXBLOCK) == GOTCRCW) {
				if (cmdzack1flg & ZCACK1)
					stohdr(0L);
				else
					loginf("request for command \"%s\" ignored",
						printable(secbuf,-32));
					stohdr(0L);
				do {
					zshhdr(4,ZCOMPL, Txhdr);
				}
				while (++errors<20 && zgethdr(Rxhdr,1) != ZFIN);
				return ackbibi();
			}
			zshhdr(4,ZNAK, Txhdr); goto again;
		case ZCOMPL:
			goto again;
		case ZRINIT:
		case ZFIN: /* do not beleive in first ZFIN */
			if (++numfin >= BELEIVE_ZFIN) return ackbibi();
			else continue;
		case ERROR:
		case HANGUP:
		case ZCAN:
			return ERROR;
		default:
			continue;
		}
	}
	return 0;
}

/*
 * Receive 1 or more files with ZMODEM protocol
 */
int rzfiles(void)
{
	register c;

	debug(11,"rzfiles");
	for (;;) {
		switch (c = rzfile()) {
		case ZEOF:
		case ZSKIP:
			switch (tryz()) {
			case ZCOMPL:
				return OK;
			default:
				return ERROR;
			case ZFILE:
				break;
			}
			continue;
		default:
			return c;
		case ERROR:
			return ERROR;
		}
	}
}

/*
 * Receive a file with ZMODEM protocol
 *  Assumes file name frame is in secbuf
 */
int rzfile(void)
{
	register c, n;

	debug(11,"rzfile");
	Eofseen=FALSE;
	rxbytes = 0l;
	if ((c = procheader(secbuf))) {
		return (tryzhdrtype = c);
	}

	n = 20;

	for (;;) {
		stohdr(rxbytes);
		zshhdr(4,ZRPOS, Txhdr);
nxthdr:
		switch (c = zgethdr(Rxhdr, 0)) {
		default:
			debug(11,"rzfile: Wrong header %d", c);
			if ( --n < 0) {
				loginf("Wrong header %d", c);
				return ERROR;
			}
			continue;
		case ZCAN:
			loginf("Sender CANcelled");
			return ERROR;
		case ZNAK:
			if ( --n < 0) {
				loginf("got ZNAK", c);
				return ERROR;
			}
			continue;
		case TIMEOUT:
			if ( --n < 0) {
				loginf("TIMEOUT", c);
				return ERROR;
			}
			continue;
		case ZFILE:
			zrdata(secbuf, MAXBLOCK);
			continue;
		case ZEOF:
			if (rclhdr(Rxhdr) != rxbytes) {
				/*
				 * Ignore eof if it's at wrong place - force
				 *  a timeout because the eof might have gone
				 *  out before we sent our zrpos.
				 */
				errors = 0;  goto nxthdr;
			}
			if (closeit(1)) {
				tryzhdrtype = ZFERR;
				logerr("Error closing file");
				return ERROR;
			}
			fout=NULL;
			debug(11,"rzfile: normal EOF");
			return c;
		case HANGUP:
			loginf("Line drop");
			return ERROR;
		case ERROR:	/* Too much garbage in header search error */
			if (--n < 0) {
				loginf("Too much errors");
				return ERROR;
			}
			zmputs(Attn);
			continue;
		case ZSKIP:
			Modtime = 1;
			closeit(1);
			loginf("Sender SKIPPED file");
			return c;
		case ZDATA:
			if (rclhdr(Rxhdr) != rxbytes) {
				if ( --n < 0) {
					loginf("Data has bad addr");
					return ERROR;
				}
				zmputs(Attn);  continue;
			}
moredata:
				debug(11,"%7ld ZMODEM%s    ",
				  rxbytes, Crc32r?" CRC-32":"");
			switch (c = zrdata(secbuf, MAXBLOCK))
			{
			case ZCAN:
				loginf("Sender CANcelled");
				return ERROR;
			case HANGUP:
				loginf("Line drop");
				return ERROR;
			case ERROR:	/* CRC error */
				if (--n < 0) {
					loginf("Too many errors");
					return ERROR;
				}
				zmputs(Attn);
				continue;
			case TIMEOUT:
				if ( --n < 0) {
					loginf("TIMEOUT");
					return ERROR;
				}
				continue;
			case GOTCRCW:
				n = 20;
				putsec(secbuf, Rxcount);
				rxbytes += Rxcount;
				stohdr(rxbytes);
				zshhdr(4,ZACK, Txhdr);
				PUTCHAR(DC1);
				goto nxthdr;
			case GOTCRCQ:
				n = 20;
				putsec(secbuf, Rxcount);
				rxbytes += Rxcount;
				stohdr(rxbytes);
				zshhdr(4,ZACK, Txhdr);
				goto moredata;
			case GOTCRCG:
				n = 20;
				putsec(secbuf, Rxcount);
				rxbytes += Rxcount;
				goto moredata;
			case GOTCRCE:
				n = 20;
				putsec(secbuf, Rxcount);
				rxbytes += Rxcount;
				goto nxthdr;
			}
		}
	}
}

/*
 * Send a string to the modem, processing for \336 (sleep 1 sec)
 *   and \335 (break signal)
 */
void zmputs(s)
char *s;
{
	register c;

	debug(11,"zmputs");
	while (*s) {
		switch (c = *s++) {
		case '\336':
			sleep(1); continue;
		case '\335':
			sendbrk(); continue;
		default:
			PUTCHAR(c);
		}
	}
}

int resync(off)
off_t off;
{
	return 0;
}

int closeit(success)
int success;
{
	int rc;

	rc=closefile(success);
	fout=NULL;
	sbytes=rxbytes-sbytes;
	(void)time(&etime);
	if ((startime=etime-startime) == 0L) startime=1L;
	loginf("%s %lu bytes in %ld seconds (%ld cps)",
		success?"received":"dropped after",
		sbytes,startime,sbytes/startime);
	return rc;
}

/*
 * Ack a ZFIN packet, let byegones be byegones
 */
int ackbibi(void)
{
	register n;
	int c;

	debug(11,"ackbibi:");
	stohdr(0L);
	for (n=3; --n>=0; ) {
		zshhdr(4,ZFIN, Txhdr);
		switch ((c=GETCHAR(10))) {
		case ZPAD:
			zgethdr(Rxhdr,0);
			debug(11,"skipped unexpected header");
			break;
		case 'O':
			GETCHAR(1);	/* Discard 2nd 'O' */
			debug(11,"ackbibi complete");
			return ZCOMPL;
		case ERROR:
		case HANGUP:
			debug(11,"ackbibi got %d, ignore",c);
			return 0;
		case TIMEOUT:
		default:
			debug(11,"ackbibi got '%s', continue",
				printablec(c));
			break;
		}
	}
	return ZCOMPL;
}

/*
 * Process incoming file information header
 */
int procheader(name)
char *name;
{
	register char *openmode, *p;
	static dummy;
	char ctt[32];

	debug(11,"proheader \"%s\"",printable(name,0));
	/* set default parameters and overrides */
	openmode = "w";

	/*
	 *  Process ZMODEM remote file management requests
	 */
	Thisbinary = (zconv != ZCNL);	/* Remote ASCII override */
	if (zmanag == ZMAPND)
		openmode = "a";

	Bytesleft = DEFBYTL; Filemode = 0; Modtime = 0L;

	p = name + 1 + strlen(name);
	sscanf(p, "%ld%lo%o%lo%d%ld%d%d",
	  &Bytesleft, &Modtime, &Filemode,
	  &dummy, &dummy, &dummy, &dummy, &dummy);
	strcpy(ctt,date(Modtime));
	loginf("zmodem receive: \"%s\" %ld bytes dated %s mode %o",
	  name, Bytesleft, ctt, Filemode);

	fout=openfile(name,Modtime,Bytesleft,&rxbytes,resync);
	(void)time(&startime);
	sbytes=rxbytes;

	if (Bytesleft == rxbytes) {
		loginf("Skipping %s", name);
		closeit(0);
		return ZSKIP;
	}
	else if ( !fout) return ZFERR;
	else return 0;
}

/*
 * Putsec writes the n characters of buf to receive file fout.
 *  If not in binary mode, carriage returns, and all characters
 *  starting with CPMEOF are discarded.
 */
int putsec(buf, n)
char *buf;
register n;
{
	register char *p;

	debug(11,"putsec %d bytes",n);
	if (n == 0)
		return OK;
	if (Thisbinary) {
		for (p=buf; --n>=0; )
			putc( *p++, fout);
	}
	else {
		if (Eofseen)
			return OK;
		for (p=buf; --n>=0; ++p ) {
			if ( *p == '\r')
				continue;
			if (*p == SUB) {
				Eofseen=TRUE; return OK;
			}
			putc(*p ,fout);
		}
	}
	return OK;
}

#if defined(HAS_STATFS) || defined(HAS_STATVFS)
long getfree(void)
{
#ifdef HAS_STATVFS
	struct statvfs sfs;

	if (statvfs(inbound,&sfs) != 0)
#else
	struct statfs sfs;

#ifdef SCO_STYLE_STATFS
	if (statfs(inbound,&sfs,sizeof(sfs),0) != 0)
#else
	if (statfs(inbound,&sfs) != 0)
#endif
#endif
	{
		logerr("$cannot statfs \"%s\", assume enough space",
			inbound);
		return -1L;
	}
	else return (sfs.f_bsize*sfs.f_bfree);
}
#else
long getfree(void)
{
	return MAXLONG;
}
#endif /* defined(HAS_STATFS) | defined(HAS_STATVFS) */

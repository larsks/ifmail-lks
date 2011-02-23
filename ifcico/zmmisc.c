/*
 *   Z M . C
 *    ZMODEM protocol primitives
 *    05-24-89  Chuck Forsberg Omen Technology Inc
 *
 * Entry point Functions:
 *	zsbhdr(type, hdr) send binary header
 *	zshhdr(type, hdr) send hex header
 *	zgethdr(hdr, eflag) receive header - binary or hex
 *	zsdata(buf, len, frameend) send data
 *	zrdata(buf, len) receive data
 *	stohdr(pos) store position data in Txhdr
 *	long rclhdr(hdr) recover position offset from header
 * 
 *
 *	This version implements numerous enhancements including ZMODEM
 *	Run Length Encoding and variable length headers.  These
 *	features were not funded by the original Telenet development
 *	contract.
 * 
 * This software may be freely used for non commercial and
 * educational (didactic only) purposes.  This software may also
 * be freely used to support file transfer operations to or from
 * licensed Omen Technology products.  Any programs which use
 * part or all of this software must be provided in source form
 * with this notice intact except by written permission from Omen
 * Technology Incorporated.
 * 
 * Use of this software for commercial or administrative purposes
 * except when exclusively limited to interfacing Omen Technology
 * products requires a per port license payment of $20.00 US per
 * port (less in quantity).  Use of this code by inclusion,
 * decompilation, reverse engineering or any other means
 * constitutes agreement to these conditions and acceptance of
 * liability to license the materials and payment of reasonable
 * legal costs necessary to enforce this license agreement.
 *
 *
 *		Omen Technology Inc		FAX: 503-621-3745
 *		Post Office Box 4681
 *		Portland OR 97208
 *
 *	This code is made available in the hope it will be useful,
 *	BUT WITHOUT ANY WARRANTY OF ANY KIND OR LIABILITY FOR ANY
 *	DAMAGES OF ANY KIND.
 *
 */

static void zputhex(int);
static void zsbh32(int,char*,int,int);
static void zsda32(char*,int,int);
static int zrdat32(char*,int);
static int noxrd7(void);
static int zrbhd32(char*);
static int zrbhdr(char*);
static int zrhhdr(char*);
static int zgethex(void);
static int zgeth1(void);
static void garbitch(void);

#include <stdio.h>
#include "xutil.h"
#include "lutil.h"
#include "ttyio.h"
#include "zmodem.h"
static int Rxtimeout = 20;	/* Tenths of seconds to wait for something */
int Zctlesc;

/* Globals used by ZMODEM functions */
int Rxframeind;		/* ZBIN ZBIN32, or ZHEX type of frame */
int Rxtype;		/* Type of header received */
int Rxhlen;		/* Length of header received */
int Rxcount;		/* Count of data bytes received */
char Rxhdr[ZMAXHLEN];	/* Received header */
char Txhdr[ZMAXHLEN];	/* Transmitted header */
long Rxpos;		/* Received file position */
long Txpos;		/* Transmitted file position */
int Txfcs32;		/* TURE means send binary frames with 32 bit FCS */
int Crc32t;		/* Controls 32 bit CRC being sent */
			/* 1 == CRC32,  2 == CRC32 + RLE */
int Crc32r;		/* Indicates/controls 32 bit CRC being received */
			/* 0 == CRC16,  1 == CRC32,  2 == CRC32 + RLE */
int Usevhdrs;		/* Use variable length headers */
int Znulls;		/* Number of nulls to send at beginning of ZDATA hdr */
char Attn[ZATTNLEN+1];	/* Attention string rx sends to tx on err */
char *Altcan;		/* Alternate canit string */

char *txbuf=NULL;
char *rxbuf=NULL;

static lastsent;	/* Last char we sent */
static Not8bit;		/* Seven bits seen on header */

char *frametypes[] = {
	"EMPTY",		/* -16 */
	"Can't be (-15)",
	"Can't be (-14)",
	"Can't be (-13)",
	"Can't be (-12)",
	"Can't be (-11)",
	"Can't be (-10)",
	"Can't be (-9)",
	"HANGUP",		/* -8 */
	"Can't be (-7)",
	"Can't be (-6)",
	"Can't be (-5)",
	"EOFILE",		/* -4 */
	"Can't be (-3)",
	"TIMEOUT",		/* -2 */
	"ERROR",		/* -1 */
	"ZRQINIT",
	"ZRINIT",
	"ZSINIT",
	"ZACK",
	"ZFILE",
	"ZSKIP",
	"ZNAK",
	"ZABORT",
	"ZFIN",
	"ZRPOS",
	"ZDATA",
	"ZEOF",
	"ZFERR",
	"ZCRC",
	"ZCHALLENGE",
	"ZCOMPL",
	"ZCAN",
	"ZFREECNT",
	"ZCOMMAND",
	"ZSTDERR",
	"xxxxx"
#define FRTYPES 22	/* Total number of frame types in this array */
			/*  not including psuedo negative entries */
};

static char badcrc[] = "Bad CRC";

/**** I am including this fix as a temporary solution... Gonna got the 
      entire zmodem rewritten some day.  = E.C.
****/

/***** Hack by mj ***********************************************************/
/*
 * Buffer for outgoing frames. Sending them with single character write()'s
 * is a waste of processor time and causes severe performance degradation
 * on TCP and ISDN connections.
 */
#define FRAME_BUFFER_SIZE	16384
static char *frame_buffer=NULL;
static int  frame_length = 0;

#define BUFFER_CLEAR()	do { frame_length=0; } while(0)
#define BUFFER_BYTE(c)	do { frame_buffer[frame_length++]=(c); } while(0)
#define BUFFER_FLUSH()	do { PUT(frame_buffer, frame_length); \
				 frame_length=0; } while(0);
/****************************************************************************/

void get_frame_buffer(void)
{
	if (frame_buffer == NULL) frame_buffer=xmalloc(FRAME_BUFFER_SIZE);
}

/* Send ZMODEM binary header hdr of type type */
void zsbhdr(len, type, hdr)
int len,type;
register char *hdr;
{
	register int n;
	register unsigned INT16 crc;

	debug(11,"zsbhdr: %c %d %s %lx", Usevhdrs?'v':'f', len,
	  frametypes[type+FTOFFSET], rclhdr(hdr));

	BUFFER_CLEAR();
	
	if (type == ZDATA)
		for (n = Znulls; --n >=0; )
			BUFFER_BYTE(0);

	BUFFER_BYTE(ZPAD); BUFFER_BYTE(ZDLE);

	switch (Crc32t=Txfcs32) {
	case 2:
		zsbh32(len, hdr, type, Usevhdrs?ZVBINR32:ZBINR32);
	case 1:
		zsbh32(len, hdr, type, Usevhdrs?ZVBIN32:ZBIN32);  break;
	default:
		if (Usevhdrs) {
			BUFFER_BYTE(ZVBIN);
			zsendline(len);
		}
		else
			BUFFER_BYTE(ZBIN);
		zsendline(type);
		crc = updcrc16(type, 0);

		for (n=len; --n >= 0; ++hdr) {
			zsendline(*hdr);
			crc = updcrc16((0377& *hdr), crc);
		}
		crc = updcrc16(0,updcrc16(0,crc));
		zsendline(crc>>8);
		zsendline(crc);
	}

	BUFFER_FLUSH();
}


/* Send ZMODEM binary header hdr of type type */
void zsbh32(len, hdr, type, flavour)
int len;
register char *hdr;
int type,flavour;
{
	register int n;
	register INT32 crc;

	BUFFER_BYTE(flavour); 
	if (Usevhdrs) 
		zsendline(len);
	zsendline(type);
	crc = 0xFFFFFFFFL; crc = updcrc32(type, crc);

	for (n=len; --n >= 0; ++hdr) {
		crc = updcrc32((0377 & *hdr), crc);
		zsendline(*hdr);
	}
	crc = ~crc;
	for (n=4; --n >= 0;) {
		zsendline((int)crc);
		crc >>= 8;
	}
}

/* Send ZMODEM HEX header hdr of type type */
void zshhdr(len, type, hdr)
int len,type;
register char *hdr;
{
	register int n;
	register unsigned INT16 crc;

	debug(11,"zshhdr: %c %d %s %lx", Usevhdrs?'v':'f', len,
	  frametypes[type+FTOFFSET], rclhdr(hdr));


	BUFFER_CLEAR();
	
	BUFFER_BYTE(ZPAD); BUFFER_BYTE(ZPAD); BUFFER_BYTE(ZDLE);
	if (Usevhdrs) {
		BUFFER_BYTE(ZVHEX);
		zputhex(len);
	}
	else
		BUFFER_BYTE(ZHEX);
	zputhex(type);
	Crc32t = 0;

	crc = updcrc16(type, 0);
	for (n=len; --n >= 0; ++hdr) {
		zputhex(*hdr); crc = updcrc16((0377 & *hdr), crc);
	}
	crc = updcrc16(0,updcrc16(0,crc));
	zputhex(crc>>8); zputhex(crc);

	/* Make it printable on remote machine */
	BUFFER_BYTE(015); BUFFER_BYTE(0212);
	/*
	 * Uncork the remote in case a fake XOFF has stopped data flow
	 */
	if (type != ZFIN && type != ZACK)
		BUFFER_BYTE(021);

	BUFFER_FLUSH();
}

/*
 * Send binary array buf of length length, with ending ZDLE sequence frameend
 */
char *Zendnames[] = { "ZCRCE", "ZCRCG", "ZCRCQ", "ZCRCW"};
void zsdata(buf, length, frameend)
register char *buf;
int length,frameend;
{
	register unsigned INT16 crc;

	debug(11,"zsdata: %d %s", length, Zendnames[(frameend-ZCRCE)&3]);


	BUFFER_CLEAR();
	
	switch (Crc32t) {
	case 1:
		zsda32(buf, length, frameend);  break;
/**
	case 2:
		zsdar32(buf, length, frameend);  break;
**/
	default:
		crc = 0;
		for (;--length >= 0; ++buf) {
			zsendline(*buf); crc = updcrc16((0377 & *buf), crc);
		}
		BUFFER_BYTE(ZDLE); BUFFER_BYTE(frameend);
		crc = updcrc16(frameend, crc);

		crc = updcrc16(0,updcrc16(0,crc));
		zsendline(crc>>8); zsendline(crc);
	}
	if (frameend == ZCRCW)
		BUFFER_BYTE(DC1);

	BUFFER_FLUSH();
}

void zsda32(buf, length, frameend)
register char *buf;
int length,frameend;
{
	register int c;
	register INT32 crc;

	crc = 0xFFFFFFFFL;
	for (;--length >= 0; ++buf) {
		c = *buf & 0377;
		if (c & 0140)
			BUFFER_BYTE(lastsent = c);
		else
			zsendline(c);
		crc = updcrc32(c, crc);
	}
	BUFFER_BYTE(ZDLE); BUFFER_BYTE(frameend);
	crc = updcrc32(frameend, crc);

	crc = ~crc;
	for (c=4; --c >= 0;) {
		zsendline((int)crc);  crc >>= 8;
	}
}

/*
 * Receive array buf of max length with ending ZDLE sequence
 *  and CRC.  Returns the ending character or error code.
 *  NB: On errors may store length+1 bytes!
 */
int zrdata(buf, length)
register char *buf;
int length;
{
	register int c;
	register unsigned INT16 crc;
	register char *end;
	register int d;

	switch (Crc32r) {
	case 1:
		return zrdat32(buf, length);
/* Case 2 was commented out, why? Anybody knows that? */
#ifndef DONT_DO_CRC_WITH_RLE
	case 2:
		return zrdatr32(buf, length);
#endif
	}

	crc = Rxcount = 0;  end = buf + length;
	while (buf <= end) {
		if ((c = zdlread()) & ~0377) {
crcfoo:
			switch (c) {
			case GOTCRCE:
			case GOTCRCG:
			case GOTCRCQ:
			case GOTCRCW:
				crc = updcrc16((((d=c))&0377), crc);
				if ((c = zdlread()) & ~0377)
					goto crcfoo;
				crc = updcrc16(c, crc);
				if ((c = zdlread()) & ~0377)
					goto crcfoo;
				crc = updcrc16(c, crc);
				if (crc & 0xFFFF) {
					loginf(badcrc);
					return ERROR;
				}
				Rxcount = length - (end - buf);

				debug(11,"zrdata: %d  %s", Rxcount,
				 Zendnames[(d-GOTCRCE)&3]);

				return d;
			case GOTCAN:
				loginf("Sender Canceled");
				return ZCAN;
			case TIMEOUT:
				loginf("TIMEOUT receiving data");
				return c;
			case HANGUP:
				loginf("Carrier lost while receiving");
				return c;
			default:
				garbitch(); return c;
			}
		}
		*buf++ = c;
		crc = updcrc16(c, crc);
	}
	loginf("Data subpacket too long");
	return ERROR;
}

int zrdat32(buf, length)
register char *buf;
int length;
{
	register int c;
	register INT32 crc;
	register char *end;
	register int d;

	crc = 0xFFFFFFFFL;  Rxcount = 0;  end = buf + length;
	while (buf <= end) {
		if ((c = zdlread()) & ~0377) {
crcfoo:
			switch (c) {
			case GOTCRCE:
			case GOTCRCG:
			case GOTCRCQ:
			case GOTCRCW:
				d = c;  c &= 0377;
				crc = updcrc32(c, crc);
				if ((c = zdlread()) & ~0377)
					goto crcfoo;
				crc = updcrc32(c, crc);
				if ((c = zdlread()) & ~0377)
					goto crcfoo;
				crc = updcrc32(c, crc);
				if ((c = zdlread()) & ~0377)
					goto crcfoo;
				crc = updcrc32(c, crc);
				if ((c = zdlread()) & ~0377)
					goto crcfoo;
				crc = updcrc32(c, crc);
				if (crc != 0xDEBB20E3) {
					loginf(badcrc);
					return ERROR;
				}
				Rxcount = length - (end - buf);

				debug(11,"zrdat32: %d %s", Rxcount,
				 Zendnames[(d-GOTCRCE)&3]);

				return d;
			case GOTCAN:
				loginf("Sender Canceled");
				return ZCAN;
			case TIMEOUT:
				loginf("TIMEOUT");
				return c;
			case HANGUP:
				loginf("Carrier lost while receiving");
				return c;
			default:
				garbitch(); return c;
			}
		}
		*buf++ = c;
		crc = updcrc32(c, crc);
	}
	loginf("Data subpacket too long");
	return ERROR;
}

void garbitch(void)
{
	loginf("Garbled data subpacket");
}

/*
 * Read a ZMODEM header to hdr, either binary or hex.
 *  eflag controls local display of non zmodem characters:
 *	0:  no display
 *	1:  display printing characters only
 *	2:  display all non ZMODEM characters
 *
 *   Set Rxhlen to size of header (default 4) (valid iff good hdr)
 *  On success, set Zmodem to 1, set Rxpos and return type of header.
 *   Otherwise return negative on error.
 *   Return ERROR instantly if ZCRCW sequence, for fast error recovery.
 */
int zgethdr(hdr, eflag)
char *hdr;
int eflag;
{
	register int c, n, cancount, tmcount;

	Rxframeind = Rxtype = 0;

startover:
	cancount = 5;
	tmcount = 5;
again:
	/* Return immediate ERROR if ZCRCW sequence seen */
	TTYWAIT(1);
	if (((c = GETCHAR(Rxtimeout)) < 0) && (c != TIMEOUT))
		goto fifi;
	else switch (c) {
	case TIMEOUT:
		if (--tmcount <= 0) {
			c = ERROR; goto fifi;
		}
		goto startover;
	case 021: case 0221:
		goto again;
	case CAN:
gotcan:
		if (--cancount <= 0) {
			c = ZCAN; goto fifi;
		}
		switch (c = GETCHAR(1)) {
		case TIMEOUT:
			goto again;
		case ZCRCW:
			switch (GETCHAR(1)) {
			case TIMEOUT:
				c = ERROR; goto fifi;
			case HANGUP:
				goto fifi;
			default:
				goto agn2;
			}
		case HANGUP:
			goto fifi;
		default:
			break;
		case CAN:
			if (--cancount <= 0) {
				c = ZCAN; goto fifi;
			}
			goto again;
		}
	/* **** FALL THRU TO **** */
	default:
agn2:
		goto startover;
	case ZPAD|0200:		/* This is what we want. */
		Not8bit = c;
	case ZPAD:		/* This is what we want. */
		break;
	}
	cancount = 5;
splat:
	switch (c = noxrd7()) {
	case ZPAD:
		goto splat;
	case HANGUP:
	case TIMEOUT:
		goto fifi;
	default:
		goto agn2;
	case ZDLE:		/* This is what we want. */
		break;
	}


	Rxhlen = 4;		/* Set default length */
	Rxframeind = c = noxrd7();
	switch (c) {
	case ZVBIN32:
		if ((Rxhlen = c = zdlread()) < 0)
			goto fifi;
		if (c > ZMAXHLEN)
			goto agn2;
		Crc32r = 1;  c = zrbhd32(hdr); break;
	case ZBIN32:
		if (Usevhdrs)
			goto agn2;
		Crc32r = 1;  c = zrbhd32(hdr); break;
	case ZVBINR32:
		if ((Rxhlen = c = zdlread()) < 0)
			goto fifi;
		if (c > ZMAXHLEN)
			goto agn2;
		Crc32r = 2;  c = zrbhd32(hdr); break;
	case ZBINR32:
		if (Usevhdrs)
			goto agn2;
		Crc32r = 2;  c = zrbhd32(hdr); break;
	case HANGUP:
	case TIMEOUT:
		goto fifi;
	case ZVBIN:
		if ((Rxhlen = c = zdlread()) < 0)
			goto fifi;
		if (c > ZMAXHLEN)
			goto agn2;
		Crc32r = 0;  c = zrbhdr(hdr); break;
	case ZBIN:
		if (Usevhdrs)
			goto agn2;
		Crc32r = 0;  c = zrbhdr(hdr); break;
	case ZVHEX:
		if ((Rxhlen = c = zgethex()) < 0)
			goto fifi;
		if (c > ZMAXHLEN)
			goto agn2;
		Crc32r = 0;  c = zrhhdr(hdr); break;
	case ZHEX:
		if (Usevhdrs)
			goto agn2;
		Crc32r = 0;  c = zrhhdr(hdr); break;
	case CAN:
		goto gotcan;
	default:
		goto agn2;
	}
	for (n = Rxhlen; ++n < ZMAXHLEN; )	/* Clear unused hdr bytes */
		hdr[n] = 0;
	Rxpos = hdr[ZP3] & 0377;
	Rxpos = (Rxpos<<8) + (hdr[ZP2] & 0377);
	Rxpos = (Rxpos<<8) + (hdr[ZP1] & 0377);
	Rxpos = (Rxpos<<8) + (hdr[ZP0] & 0377);
fifi:
	switch (c) {
	case GOTCAN:
		c = ZCAN;
	/* **** FALL THRU TO **** */
	case ZNAK:
	case ZCAN:
	case ERROR:
	case TIMEOUT:
	case HANGUP:
	default:
		if (c >= -FTOFFSET && c <= FRTYPES)
			debug(11,"zgethdr: %c %d %s %lx", Rxframeind, Rxhlen,
			  frametypes[c+FTOFFSET], Rxpos);
		else
			debug(11,"zgethdr: %c %d %lx", Rxframeind, c, Rxpos);
	}
	/* Use variable length headers if we got one */
	if (c >= 0 && c <= FRTYPES && Rxframeind & 040)
		Usevhdrs = 1;
	return c;
}

/* Receive a binary style header (type and position) */
int zrbhdr(hdr)
register char *hdr;
{
	register int c, n;
	register unsigned INT16 crc;

	if ((c = zdlread()) & ~0377)
		return c;
	Rxtype = c;
	crc = updcrc16(c, 0);

	for (n=Rxhlen; --n >= 0; ++hdr) {
		if ((c = zdlread()) & ~0377)
			return c;
		crc = updcrc16(c, crc);
		*hdr = c;
	}
	if ((c = zdlread()) & ~0377)
		return c;
	crc = updcrc16(c, crc);
	if ((c = zdlread()) & ~0377)
		return c;
	crc = updcrc16(c, crc);
	if (crc & 0xFFFF) {
		loginf(badcrc);
		return ERROR;
	}
	return Rxtype;
}

/* Receive a binary style header (type and position) with 32 bit FCS */
int zrbhd32(hdr)
register char *hdr;
{
	register int c, n;
	register INT32 crc;

	if ((c = zdlread()) & ~0377)
		return c;
	Rxtype = c;
	crc = 0xFFFFFFFFL; crc = updcrc32(c, crc);

	debug(11,"zrbhd32 c=%X  crc=%lX", c, crc);

	for (n=Rxhlen; --n >= 0; ++hdr) {
		if ((c = zdlread()) & ~0377)
			return c;
		crc = updcrc32(c, crc);
		*hdr = c;

		debug(11,"zrbhd32 c=%X  crc=%lX", c, crc);
	}
	for (n=4; --n >= 0;) {
		if ((c = zdlread()) & ~0377)
			return c;
		crc = updcrc32(c, crc);

		debug(11,"zrbhd32 c=%X  crc=%lX", c, crc);
	}
	if (crc != 0xDEBB20E3) {
		loginf(badcrc);
		return ERROR;
	}
	return Rxtype;
}


/* Receive a hex style header (type and position) */
int zrhhdr(hdr)
char *hdr;
{
	register int c;
	register unsigned INT16 crc;
	register int n;

	if ((c = zgethex()) < 0)
		return c;
	Rxtype = c;
	crc = updcrc16(c, 0);

	for (n=Rxhlen; --n >= 0; ++hdr) {
		if ((c = zgethex()) < 0)
			return c;
		crc = updcrc16(c, crc);
		*hdr = c;
	}
	if ((c = zgethex()) < 0)
		return c;
	crc = updcrc16(c, crc);
	if ((c = zgethex()) < 0)
		return c;
	crc = updcrc16(c, crc);
	if (crc & 0xFFFF) {
		loginf(badcrc); return ERROR;
	}
	switch ( c = GETCHAR(2)) {
	case 0215:
		Not8bit = c;
		/* **** FALL THRU TO **** */
	case 015:
	 	/* Throw away possible cr/lf */
		switch (c = GETCHAR(2)) {
		case 012:
			Not8bit |= c;
		}
	}
	if (c < 0)
		return c;
	return Rxtype;
}

/* Send a byte as two hex digits */
void zputhex(c)
register int c;
{
	static char	digits[]	= "0123456789abcdef";

	debug(19,"zputhex: %02X", c);

	BUFFER_BYTE(digits[(c&0xF0)>>4]);
	BUFFER_BYTE(digits[(c)&0xF]);
}

/*
 * Send character c with ZMODEM escape sequence encoding.
 *  Escape XON, XOFF. Escape CR following @ (Telenet net escape)
 */
void zsendline(c)
int c;
{

	/* Quick check for non control characters */
	if (c & 0140)
		BUFFER_BYTE(lastsent = c);
	else {
		switch (c &= 0377) {
		case ZDLE:
			BUFFER_BYTE(ZDLE);
			BUFFER_BYTE (lastsent = (c ^= 0100));
			break;
		case 015:
		case 0215:
			if (!Zctlesc && (lastsent & 0177) != '@')
				goto sendit;
		/* **** FALL THRU TO **** */
		case 020:
		case 021:
		case 023:
		case 0220:
		case 0221:
		case 0223:
			BUFFER_BYTE(ZDLE);
			c ^= 0100;
	sendit:
			BUFFER_BYTE(lastsent = c);
			break;
		default:
			if (Zctlesc && ! (c & 0140)) {
				BUFFER_BYTE(ZDLE);
				c ^= 0100;
			}
			BUFFER_BYTE(lastsent = c);
		}
	}
}

/* Decode two lower case hex digits into an 8 bit byte value */
int zgethex(void)
{
	register int c;

	c = zgeth1();
	debug(19,"zgethex: %02X", c);
	return c;
}
int zgeth1(void)
{
	register int c, n;

	if ((c = noxrd7()) < 0)
		return c;
	n = c - '0';
	if (n > 9)
		n -= ('a' - ':');
	if (n & ~0xF)
		return ERROR;
	if ((c = noxrd7()) < 0)
		return c;
	c -= '0';
	if (c > 9)
		c -= ('a' - ':');
	if (c & ~0xF)
		return ERROR;
	c += (n<<4);
	return c;
}

/*
 * Read a byte, checking for ZMODEM escape encoding
 *  including CAN*5 which represents a quick abort
 */
int zdlread(void)
{
	register int c;

again:
	/* Quick check for non control characters */
	if ((c = GETCHAR(Rxtimeout)) & 0140)
		return c;
	switch (c) {
	case ZDLE:
		break;
	case 023:
	case 0223:
	case 021:
	case 0221:
		goto again;
	default:
		if (Zctlesc && !(c & 0140)) {
			goto again;
		}
		return c;
	}
again2:
	if ((c = GETCHAR(Rxtimeout)) < 0)
		return c;
	if (c == CAN && (c = GETCHAR(Rxtimeout)) < 0)
		return c;
	if (c == CAN && (c = GETCHAR(Rxtimeout)) < 0)
		return c;
	if (c == CAN && (c = GETCHAR(Rxtimeout)) < 0)
		return c;
	switch (c) {
	case CAN:
		return GOTCAN;
	case ZCRCE:
	case ZCRCG:
	case ZCRCQ:
	case ZCRCW:
		return (c | GOTOR);
	case ZRUB0:
		return 0177;
	case ZRUB1:
		return 0377;
	case 023:
	case 0223:
	case 021:
	case 0221:
		goto again2;
	default:
		if (Zctlesc && ! (c & 0140)) {
			goto again2;
		}
		if ((c & 0140) ==  0100)
			return (c ^ 0100);
		break;
	}
	debug(11,"Bad escape sequence %x", c);
	return ERROR;
}

/*
 * Read a character from the modem line with timeout.
 *  Eat parity, XON and XOFF characters.
 */
int noxrd7(void)
{
	register int c;

	for (;;) {
		if ((c = GETCHAR(Rxtimeout)) < 0)
			return c;
		switch (c &= 0177) {
		case DC1:
		case DC3:
			continue;
		default:
			if (Zctlesc && !(c & 0140))
				continue;
		case '\r':
		case '\n':
		case ZDLE:
			return c;
		}
	}
}

/* Store long integer pos in Txhdr */
void stohdr(pos)
long pos;
{
	Txhdr[ZP0] = pos;
	Txhdr[ZP1] = pos>>8;
	Txhdr[ZP2] = pos>>16;
	Txhdr[ZP3] = pos>>24;
}

/* Recover a long integer from a header */
long rclhdr(hdr)
register char *hdr;
{
	register long l;

	l = (hdr[ZP3] & 0377);
	l = (l << 8) | (hdr[ZP2] & 0377);
	l = (l << 8) | (hdr[ZP1] & 0377);
	l = (l << 8) | (hdr[ZP0] & 0377);
	return l;
}

/* End of zmmisc.c */

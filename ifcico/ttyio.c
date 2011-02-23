#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#if defined(HAS_TCP) || defined(HAS_TERM)
#ifdef HAS_NET_ERRNO_H
#include <sys/bsdtypes.h>
#include <net/errno.h>
#endif
#endif
#ifdef HAS_IOCTL_H
#include <sys/ioctl.h>
#endif
#include <sys/socket.h>
#include <fcntl.h>

#ifdef NOISEDEBUG
#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <time.h>
#endif

#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include "lutil.h"
#include "ttyio.h"

#define PUSHMAX 8

extern int hanged_up;

int timed_out;
int tty_status=0;
int f_flags;

#ifdef NOISEDEBUG
extern int junklevel;
extern void damage(char*,int*);
#endif

static int tty_wait(void);
static int tty_nowait(void);

static char buf[BUFSIZ];
static char *next;
static int left=0;

static char pushbuf[PUSHMAX];
static int pushindex;

static void almhdl(sig)
int sig;
{
	debug(18,"timeout");
	timed_out=1;
}

static void get_buf(tot)
int tot;
{
	int err;

	if (tty_status&STAT_HANGUP) return;

	timed_out=0;
	signal(SIGALRM,almhdl);
	alarm(tot);
	left=read(0,buf,BUFSIZ);
	if (left < 0) err=errno;
	else err=0;
#ifdef NOISEDEBUG
	damage(buf,&left);
#endif
	alarm(0);
	signal(SIGALRM,SIG_DFL);
	if (hanged_up)
		tty_status=STAT_HANGUP;
	else if (timed_out)
		tty_status=STAT_TIMEOUT;
	else if (left == 0)
		tty_status=STAT_EMPTY;
	else if (left < 0)
	{
		if ((err == 0) || (err == EAGAIN))
			tty_status=STAT_EMPTY;
		else if ((err == EPIPE)
#if defined(HAS_TCP) || defined(HAS_TERM)
					|| (err == ECONNRESET)
#endif
							      )
			tty_status=STAT_HANGUP;
		else
			tty_status=STAT_ERROR;
	}
	else
		tty_status=0;
	if (tty_status & STAT_ERROR) logerr("$tty_getc error");
	if (tty_status) left=0;
	next=buf;
	debug(19,"get_buf returning %d bytes, status=0x%02x",left,tty_status);
}

int tty_ungetc(c)
int c;
{
	if (pushindex >= PUSHMAX) return -1;
	pushbuf[pushindex++]=c;
	return 0;
}

int tty_getc(tot)
int tot;
{
	int c;

	if (pushindex) return pushbuf[--pushindex]&0xff;

	if (!left) get_buf(tot);
	if (tty_status) c=-tty_status;
	else
	{
		left--;
		c=(*(next++))&0xff;
	}
	if (c < 0) debug(19,"getc return %d",c);
	else debug(19,"getc return '%s'",printablec(c));
	return c;
}

int tty_get(str,l,tot)
char *str;
int l,tot;
{
	int got,err;

	if (pushindex >= l)
	{
		while (l)
		{
			*(str++)=pushbuf[--pushindex];
			l--;
		}
		return 0;
	}

	while (pushindex)
	{
		*(str++)=pushbuf[--pushindex];
		l--;
	}

	if (left >= l)
	{
		memcpy(str,next,l);
		next+=l;
		left-=l;
		return 0;
	}

	if (left > 0)
	{
		memcpy(str,next,left);
		str+=left;
		next+=left;
		l-=left;
		left=0;
	}

	if (tty_status&STAT_HANGUP) return tty_status;

	timed_out=0;
	signal(SIGALRM,almhdl);
	alarm(tot);
	while (l && (tty_status == 0))
	{
		got=read(0,str,l);
		if (got < 0) err=errno;
		else err=0;
#ifdef NOISEDEBUG
		damage(str,&got);
#endif
		if (hanged_up)
			tty_status=STAT_HANGUP;
		else if (timed_out)
			tty_status=STAT_TIMEOUT;
		else if (got == 0)
			tty_status=STAT_EMPTY;
		else if (got < 0)
		{
			if ((err == 0) || (err == EAGAIN))
				tty_status |= STAT_EMPTY;
			else if ((err == EPIPE)
#if defined(HAS_TCP) || defined(HAS_TERM)
						|| (err == ECONNRESET)
#endif
								      )
				tty_status |= STAT_HANGUP;
			else
				tty_status=STAT_ERROR;
		}
		else
			tty_status=0;
		if (tty_status & STAT_ERROR) logerr("$tty_get error");
		str+=got;
		l-=got;
	}
	alarm(0);
	signal(SIGALRM,SIG_DFL);
	debug(19,"tty_get %d bytes missing, status=0x%02x",l,tty_status);
	return tty_status;
}

int tty_putc(c)
int c;
{
	char buf;

	if (tty_status&STAT_HANGUP) return -tty_status;
	buf=c&0xff;
	tty_status=0;
	ttywait(1);
	if (write(1,&buf,1) != 1)
	{
		if ((hanged_up) ||
		    (errno == EPIPE) 
#if defined(HAS_TCP) || defined(HAS_TERM)
				     || (errno == ECONNRESET)
#endif
							    )
			tty_status=STAT_HANGUP;
		else tty_status=STAT_ERROR;
	}
	if (tty_status) logerr("$tty_putc status=0x%02x",tty_status);
	else debug(19,"putc '%s'",printablec(c));
	return -tty_status;
}

int tty_put(str,l)
char *str;
int l;
{
	int written;

	if (tty_status&STAT_HANGUP) return -tty_status;
	tty_status=0;
	if (l <= 0) return 0;
	ttywait(1);
	if ((written=write(1,str,l)) != l)
	{
		if ((hanged_up) ||
		    (errno == EPIPE)
#if defined(HAS_TCP) || defined(HAS_TERM)
				     || (errno == ECONNRESET)
#endif
							     )
			tty_status=STAT_HANGUP;
		else tty_status=STAT_ERROR;
	}
	if (tty_status)
	{
		int tmpfl=fcntl(1,F_GETFL,0L);
		logerr("$tty_put status=0x%02x (req %d, wrote %d, fl 0%o)",
				tty_status,l,written,tmpfl);
	}
	else debug(19,"tty_put %d bytes",l);
	return -tty_status;
}

static int waitmode=1;

int ttywait(on)
int on;
{
	int rc=0;

	if (on == waitmode) return 0;
	debug(18,"setting port waitmode %s",on?"on":"off");
	if (on) rc=tty_wait();
	else rc=tty_nowait();
	if (rc == 0) waitmode=on;
	return rc;
}

int tty_wait(void)
{
	int rc;

	if ((rc=fcntl(0,F_SETFL,f_flags & ~O_NDELAY)))
	{
		logerr("$SETFL normal ioctl error");
	}
	return rc;
}

int tty_nowait(void)
{
	int rc;

	if ((rc=fcntl(0,F_SETFL,f_flags | O_NDELAY)))
	{
		logerr("$SETFL nonblock ioctl error");
	}
	return rc;
}

#ifdef NOISEDEBUG

#ifndef RAND_MAX
#ifdef INT_MAX
#define RAND_MAX INT_MAX
#else
#define RAND_MAX 2147483647
#endif
#endif

void damage(buf,len)
char *buf;
int *len;
{
	long width;
	int i;

	if ((junklevel == 0) || (*len == 0)) return;

	width=RAND_MAX/junklevel;
	if (rand() < width)
	{
		if (rand() & 4)
		{
			debug(18,"length %d damaged - decreased (if > 1)",*len);
			if ((*len) > 1) (*len)--;
		}
		else
		{
			debug(18,"length %d damaged - increased",*len);
			(*len)++;
		}
	}
	else for (i=0;i<*len;i++) if (rand() < width)
	{
		debug(18,"char %d of %d damaged",i,*len);
		buf[i]=rand();
	}
}

#endif

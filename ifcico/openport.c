#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef HAS_IOCTL_H
#include <sys/ioctl.h>
#endif
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#if defined(HAS_TERMIOS_H)
#include <termios.h>
#elif defined(HAS_TERMIO_H)
#include <termio.h>
#define speed_t int
#elif defined(HAS_SGTTY_H)
#include <sgtty.h>
#else
#error must have termios.h, termio.h or sgtty.h
#endif
#include "lutil.h"
#include "xutil.h"
#include "ulock.h"
#include "ttyio.h"
#ifdef HAS_DIAL
#ifdef HAS_DIAL_H
#include <dial.h>
#else

typedef struct {
	void *attr;	/* struct termio may be undefined */
	int baud;	/* unused */
	int speed;	/* 212A modem: low=300, high=1200 */
	char *line;	/* device name for out-going line */
	char *telno;	/* ptr to tel-no/system name string */
	int modem;	/* unused */
	char *device;	/* pointer to a CALL_EXT structure */
			/* this was unused previously */
	int dev_len;	/* unused */
} CALL;

extern int dial();
extern void undial();

#endif /* HAS_DIAL_H */

static void dialerr();

#endif /* HAS_DIAL */

static char *openedport=NULL,*pname;
static int need_detach=1;

extern int f_flags;

int openport(char*,int);
void localport(void);
void nolocalport(void);
void closeport(void);
int tty_raw(int);
int tty_local(void);
int tty_nolocal(void);
int tty_cooked(void);
void linedrop(int);
void interrupt(int);

#if defined(HAS_TERMIO_H) || defined(HAS_TERMIOS_H)
static speed_t transpeed(int);
#endif

int hanged_up;

void linedrop(sig)
int sig;
{
	loginf("got SIGHUP");
	hanged_up=1;
	return;
}

void interrupt(sig)
int sig;
{
	loginf("got SIGINT");
	signal(SIGINT,interrupt);
	return;
}

#ifdef TIOCWONLINE

#define WONLINE_TIMEOUT 5

static void alarmsig(sig)
int sig;
{
	loginf("got TIMEOUT waiting for carrier");
	hanged_up=1; /* probably the modem hung up before raising DCD */
	return;
}

#endif

int openport(port,speed)
char *port;
int speed;
{
	int rc;
	char *errtty=NULL;
	int fd;
	int outflags;
#ifdef HAS_DIAL
#ifdef CS_PROB
	CALL_EXT callx;
	char protocols[64];
#endif
	CALL call;

	memset(&call,0,sizeof(call));
#ifdef CS_PROB
	memset(&callx,0,sizeof(callx));
	callx.service="ifcico";
	callx.protocol=protocols;
	call.device=(char*)&callx;
#endif
	call.attr=0;
	call.baud=-1;
	call.telno=NULL;
#endif

	debug(18,"try opening port \"%s\" at %d",S(port),speed);
	if (openedport) free(openedport);
	if (port[0] == '/') openedport=xstrcpy(port);
	else
	{
		openedport=xstrcpy("/dev/");
		openedport=xstrcat(openedport,port);
	}
	pname=strrchr(openedport,'/');

#ifndef HAS_DIAL
	if ((rc=lock(pname)))
	{
		free(openedport);
		openedport=NULL;
		return rc;
	}
#endif

	if (need_detach)
	{
		fflush(stdin);
		fflush(stdout);
		setbuf(stdin,NULL);
		setbuf(stdout,NULL);
		close(0);
		close(1);
		if ((errtty=ttyname(2)))
		{
			debug(18,"stderr was on \"%s\", closing",errtty);
			fflush(stderr);
			close(2);
		}
#ifdef HAS_SETSID
		if (getpid() != getpgrp()) /* Already session leader? */
		{
			if (setsid() < 0)
			{
				logerr("$setsid failed");
				ulock(pname);
				return 1;
			}
		}
#else
#ifdef BSD_SETPGRP
		if (setpgrp(0,0) < 0)
#else
		if (setpgrp() < 0)
#endif
		{
			logerr("$setpgrp failed");
			ulock(pname);
			return 1;
		}
#endif
	}
	tty_status=0;
	hanged_up=0;
	signal(SIGHUP,linedrop);
	signal(SIGPIPE,linedrop);
	signal(SIGINT,interrupt);
	rc = 0;
	debug(18,"try open");
#ifdef HAS_DIAL
	call.line=openedport;
	if ((fd=dial(call)) < 0)
	{
		rc=1;
		dialerr(fd);
		fd=open("/dev/null",O_RDONLY);
	}
	else if (fd > 0)
		if (dup(fd) != 0)
		{
			logerr("cannot dup fd %d to 0 (fd 0 busy?)",fd);
			rc=1;
		}
		else
		{
			close(fd);
			fd=0;
		}
	if (dup(fd) != 1)
	{
		rc=1;
		logerr("$cannot dup stdin to stdout");
		fd=open("/dev/null",O_WRONLY);
	}
#else
	if ((fd=open(openedport,O_RDONLY|O_NDELAY)) != 0)
	{
		rc=1;
		loginf("$cannot open \"%s\" as stdin",S(openedport));
		fd=open("/dev/null",O_RDONLY);
	}
	if ((fd=open(openedport,O_WRONLY|O_NDELAY)) != 1)
	{
		rc=1;
		loginf("$cannot open \"%s\" as stdout",S(openedport));
		fd=open("/dev/null",O_WRONLY);
	}
#endif
	clearerr(stdin);
	clearerr(stdout);
	if (need_detach)
	{
#ifdef TIOCSCTTY
		if (ioctl(0,TIOCSCTTY,1L) < 0)
		{
			logerr("$TIOCSCTTY failed");
		}
#endif
		if (errtty)
		{
			rc = rc || (open(errtty,O_WRONLY) != 2);
		}
		need_detach=0;
	}
	debug(18,"after open rc=%d",rc);
	if (rc) loginf("cannot switch i/o to port \"%s\"",S(openedport));
	else
	{
		if (tty_raw(speed))
		{
			logerr("$cannot set raw mode for \"%s\"",S(openedport));
			rc=1;
		}

		if (((f_flags=fcntl(0,F_GETFL,0L)) == -1) ||
		    ((outflags=fcntl(1,F_GETFL,0L)) == -1))
		{
			rc=1;
			logerr("$GETFL error");
			f_flags=0;
			outflags=0;
		}
		else
		{
			debug(18,"return to blocking mode");
			f_flags &= ~O_NDELAY;
			outflags &= ~O_NDELAY;
			if ((fcntl(0,F_SETFL,f_flags) != 0) ||
			    (fcntl(1,F_SETFL,outflags) != 0))
			{
				rc=1;
				logerr("$SETFL error");
			}
		}
		debug(18,"file flags: stdin: 0x%04x, stdout: 0x%04x",
			f_flags,outflags);
	}

	if (rc) closeport();
	return rc;
}

void localport(void)
{
	debug(18,"setting port \"%s\" local",S(openedport));
	signal(SIGHUP,SIG_IGN);
	signal(SIGPIPE,SIG_IGN);
	if (isatty(0)) tty_local();
	return;
}

void nolocalport(void)
{
	debug(18,"setting port \"%s\" non-local",S(openedport));
	if (isatty(0)) tty_nolocal();
	return;
}

int rawport(void)
{
	tty_status=0;
	signal(SIGHUP,linedrop);
	signal(SIGPIPE,linedrop);
	if (isatty(0)) return tty_raw(0);
	else return 0;
}

int cookedport(void)
{
	signal(SIGHUP,SIG_IGN);
	signal(SIGPIPE,SIG_IGN);
	if (isatty(0)) return tty_cooked();
	else return 0;
}

void closeport(void)
{
	debug(18,"closing port \"%s\"",S(openedport));
	fflush(stdin);
	fflush(stdout);
	tty_cooked();
#ifdef HAS_DIAL
	close(1);
	undial(0);
#else
	close(0);
	close(1);
	ulock(pname);
#endif
	if (openedport) free(openedport);
	openedport=NULL;
	return;
}

void sendbrk(void)
{
	if (isatty(0))
#if (defined(TIOCSBRK))
	ioctl(0, TIOCSBRK, 0L);
#elif (defined(TCSBRK))
	ioctl(0, TCSBRK, 0L);
#else /* any ideas about BSD? */
	;
#endif
}

#if defined(HAS_TERMIO_H)

static struct termio savetio;
static struct termio tio;

int tty_raw(speed)
int speed;
{
	int rc;
	speed_t tspeed;

	debug(18,"set tty raw");
	tspeed=transpeed(speed);
	if ((rc=ioctl(0,TCGETA,&savetio)) != 0)
	{
		logerr("$ioctl(TCGETA) return %d",rc);
		return rc;
	}
	else
	{
		debug(18,"savetio.c_iflag=0x%08x",savetio.c_iflag);
		debug(18,"savetio.c_oflag=0x%08x",savetio.c_oflag);
		debug(18,"savetio.c_cflag=0x%08x",savetio.c_cflag);
		debug(18,"savetio.c_lflag=0x%08x",savetio.c_lflag);
		debug(18,"savetio.c_cc=\"%s\"",printable(savetio.c_cc,NCC));
		debug(18,"file flags: stdin: 0x%04x, stdout: 0x%04x",
			fcntl(0,F_GETFL,0L),fcntl(1,F_GETFL,0L));
	}
	tio=savetio;
	tio.c_iflag=0;
	tio.c_oflag=0;
	tio.c_cflag &= ~(CSTOPB | PARENB | PARODD);
	tio.c_cflag |= CS8 | CREAD | HUPCL | CLOCAL;
	if (tspeed)
	{
		tio.c_cflag &= ~CBAUD;
		tio.c_cflag |= tspeed;
	}
	tio.c_lflag=0;
	tio.c_cc[VMIN]=1;
	tio.c_cc[VTIME]=0;
	if ((rc=ioctl(0,TCSETAW,&tio)))
		logerr("$ioctl(0,TCSETAW,raw) return %d",rc);
	return rc;
}

int tty_local(void)
{
	struct termio tio;
	long cflag;
	int rc;

	if ((rc=ioctl(0,TCGETA,&tio)))
	{
		logerr("$ioctl(TCGETA,save) return %d",rc);
		return rc;
	}
	cflag=tio.c_cflag | CLOCAL;

	tio.c_cflag &= ~CBAUD;
	if ((rc=ioctl(0,TCSETAW,&tio)))
		logerr("$ioctl(0,TCSETAW,hangup) return %d",rc);

	sleep(1); /* as far as I notice, DTR goes back high on next op. */

	tio.c_cflag=cflag;
	if ((rc=ioctl(0,TCSETAW,&tio)))
		logerr("$ioctl(0,TCSETAW,clocal) return %d",rc);
	return rc;
}

int tty_nolocal(void)
{
	struct termio tio;
	int rc;

	if ((rc=ioctl(0,TCGETA,&tio)))
	{
		logerr("$ioctl(TCGETA,save) return %d",rc);
		return rc;
	}
	tio.c_cflag &= ~CLOCAL;
#if defined(CRTSCTS)
	tio.c_cflag |= CRTSCTS;
#elif defined(CRTSFL)
	tio.c_cflag |= CRTSFL;
#else /* donno what can be done here... Protocols won't run w/o hw flow ctl. */
	tio.c_cflag |= 0;
#endif

	if ((rc=ioctl(0,TCSETAW,&tio)))
		logerr("$ioctl(0,TCSETAW,clocal) return %d",rc);
	return rc;
}

int tty_cooked(void)
{
	int rc;

	if ((rc=ioctl(0,TCSETAF,&savetio)) != 0)
		logerr("$ioctl(TCSETAF) return %d",rc);
	return rc;
}

#elif defined(HAS_TERMIOS_H)

static struct termios savetios;
static struct termios tios;

int tty_raw(speed)
int speed;
{
	int rc;
	speed_t tspeed;

	debug(18,"set tty raw");
	tspeed=transpeed(speed);
	if ((rc=tcgetattr(0,&savetios)))
	{
		logerr("$tcgetattr(0,save) return %d",rc);
		return rc;
	}
	else
	{
		debug(18,"savetios.c_iflag=0x%08x",savetios.c_iflag);
		debug(18,"savetios.c_oflag=0x%08x",savetios.c_oflag);
		debug(18,"savetios.c_cflag=0x%08x",savetios.c_cflag);
		debug(18,"savetios.c_lflag=0x%08x",savetios.c_lflag);
		debug(18,"savetios.c_cc=\"%s\"",printable(savetios.c_cc,NCCS));
		debug(18,"file flags: stdin: 0x%04x, stdout: 0x%04x",
			fcntl(0,F_GETFL,0L),fcntl(1,F_GETFL,0L));
	}
	tios=savetios;
	tios.c_iflag=0;
	tios.c_oflag=0;
	tios.c_cflag &= ~(CSTOPB | PARENB | PARODD);
	tios.c_cflag |= CS8 | CREAD | HUPCL | CLOCAL;
	tios.c_lflag=0;
	tios.c_cc[VMIN]=1;
	tios.c_cc[VTIME]=0;
	if (tspeed)
	{
		cfsetispeed(&tios,tspeed);
		cfsetospeed(&tios,tspeed);
	}
	if ((rc=tcsetattr(0,TCSADRAIN,&tios)))
		logerr("$tcsetattr(0,TCSADRAIN,raw) return %d",rc);
	return rc;
}

int tty_local(void)
{
	struct termios tios;
	tcflag_t cflag;
	speed_t ispeed,ospeed;
	int rc;

	if ((rc=tcgetattr(0,&tios)))
	{
		logerr("$tcgetattr(0,save) return %d",rc);
		return rc;
	}
	cflag=tios.c_cflag | CLOCAL;

	ispeed=cfgetispeed(&tios);
	ospeed=cfgetospeed(&tios);
	cfsetispeed(&tios,0);
	cfsetospeed(&tios,0);
	if ((rc=tcsetattr(0,TCSADRAIN,&tios)))
		logerr("$tcsetattr(0,TCSADRAIN,hangup) return %d",rc);

	sleep(1); /* as far as I notice, DTR goes back high on next op. */

	tios.c_cflag=cflag;
	cfsetispeed(&tios,ispeed);
	cfsetospeed(&tios,ospeed);
	if ((rc=tcsetattr(0,TCSADRAIN,&tios)))
		logerr("$tcsetattr(0,TCSADRAIN,clocal) return %d",rc);
	return rc;
}

int tty_nolocal(void)
{
	struct termios tios;
	int rc;

	if ((rc=tcgetattr(0,&tios)))
	{
		logerr("$tcgetattr(0,save) return %d",rc);
		return rc;
	}
	tios.c_cflag &= ~CLOCAL;
#if defined(CRTSCTS)
	tios.c_cflag |= CRTSCTS;
#elif defined(CRTSFL)
	tios.c_cflag |= CRTSFL;
#else /* donno what can be done here... Protocols won't run w/o hw flow ctl. */
	tios.c_cflag |= 0;
#endif

	if ((rc=tcsetattr(0,TCSADRAIN,&tios)))
		logerr("$tcsetattr(0,TCSADRAIN,clocal) return %d",rc);
#ifdef TIOCWONLINE
	/* on the dialout, wait for carrier here.  I hope that this
	   won't do harm when the modem is already connected, when
	   tty_nolocal() is called while incoming session... */
	signal(SIGALRM,alarmsig);
	alarm(WONLINE_TIMEOUT);
	(void)ioctl(0,TIOCWONLINE,0L);
	alarm(0);
	signal(SIGALRM,SIG_DFL);
#endif
	return rc;
}

int tty_cooked(void)
{
	int rc;

	if ((rc=tcsetattr(0,TCSAFLUSH,&savetios)))
		logerr("$tcsetattr(0,TCSAFLUSH,save) return %d",rc);
	return rc;
}

#elif defined(HAS_SGTTY_H)

/* Never used BSD style terminal control, did you? */
/* OK, it seems that vladimir@c-press.udm.ru (Vladimir Zarozhevsky) */
/* did the job...  I did not check it. =EC */

static struct sgttyb	sg, ng;

int tty_raw(speed)
int speed;
{
int	iparam;

	debug(18,"set tty raw speed:%d", speed);
	if (ioctl(0, TIOCGETP, &sg) != 0) {
		debug(2,"tty_raw: error TIOCGETP");
		return 1;
	}
	ng = sg;
	ng.sg_flags = RAW;
/*	tios.sg_flags |= NOHANG;*/
/*	tios.sg_flags &=~ECHO;	*/	/* ECHO disable */
/*	tios.sg_flags &=~TANDEM;*/	/* XON disable */
	ioctl(0, TIOCSETP, &ng);
	if (ioctl(0, TIOCSETP, &ng) != 0) {
		debug(2,"tty_raw: error TIOCSETP");
		return 1;
	}
	return (0);
}

int tty_cooked(void)
{
	return (ioctl(0, TIOCSETN, &sg));
}

int tty_local(void)
{
	return 0;
}

int tty_nolocal(void)
{
	return 0;
}

#endif /* types of terminal control */

#ifdef HAS_DIAL

static void dialerr(fd)
int fd;
{
	char *em;

	switch (fd)
	{
	case INTRPT:	em="interrupt occured"; break;
	case D_HUNG:	em="dialer hung (no return from write)"; break;
	case NO_ANS:	em="no answer within 10 seconds"; break;
	case ILL_BD:	em="illegal baud rate"; break;
	case A_PROB:	em="acu problem (open() failure)"; break;
	case L_PROB:	em="line problem (open() failure)"; break;
	case NO_Ldv:	em="can't open LDEVS file"; break;
	case DV_NT_A:	em="requested device not available"; break;
	case DV_NT_K:	em="requested device not known"; break;
	case NO_BD_A:	em="no device available at requestd baud"; break;
	case NO_BD_K:	em="no device known at requestd baud"; break;
	case DV_NT_E:	em="requested speed doesn't match"; break;
	case BAD_SYS:	em="system not in System file"; break;
#ifdef CS_PROB
	case CS_PROB:	em="communication server problem"; break;
#endif
	
	default:	em="unknown error code"; break;
	}
	loginf("dial error\n\tfd=%d : %s",fd,em);
}

#endif

#if defined(HAS_TERMIO_H) || defined(HAS_TERMIOS_H)
speed_t transpeed(speed)
int speed;
{
	speed_t tspeed;

	switch (speed)
	{
	case 0:		tspeed=0; break;
#if defined(B50)
	case 50:	tspeed=B50; break;
#endif
#if defined(B75)
	case 75:	tspeed=B75; break;
#endif
#if defined(B110)
	case 110:	tspeed=B110; break;
#endif
#if defined(B134)
	case 134:	tspeed=B134; break;
#endif
#if defined(B150)
	case 150:	tspeed=B150; break;
#endif
#if defined(B200)
	case 200:	tspeed=B200; break;
#endif
#if defined(B300)
	case 300:	tspeed=B300; break;
#endif
#if defined(B600)
	case 600:	tspeed=B600; break;
#endif
#if defined(B1200)
	case 1200:	tspeed=B1200; break;
#endif
#if defined(B1800)
	case 1800:	tspeed=B1800; break;
#endif
#if defined(B2400)
	case 2400:	tspeed=B2400; break;
#endif
#if defined(B4800)
	case 4800:	tspeed=B4800; break;
#endif
#if defined(B7200)
	case 7200:	tspeed=B7200; break;
#endif
#if defined(B9600)
	case 9600:	tspeed=B9600; break;
#endif
#if defined(B12000)
	case 12000:	tspeed=B12000; break;
#endif
#if defined(B14400)
	case 14400:	tspeed=B14400; break;
#endif
#if defined(B19200)
	case 19200:	tspeed=B19200; break;
#elif defined(EXTA)
	case 19200:	tspeed=EXTA; break;
#endif
#if defined(B38400)
	case 38400:	tspeed=B38400; break;
#elif defined(EXTB)
	case 38400:	tspeed=EXTB; break;
#endif
#if defined(B57600)
	case 57600:	tspeed=B57600; break;
#endif
#if defined(B115200)
	case 115200:	tspeed=B115200; break;
#endif
	default:	logerr("requested invalid speed %d",speed);
			tspeed=0; break;
	}

	return tspeed;
}
#endif

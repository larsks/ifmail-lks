#ifdef HAS_TCP

#include <sys/types.h>
#include <sys/param.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "lutil.h"

#define FIDOPORT 60179		/* my birthday */

extern void linedrop(int);

int opentcp(char*);
void closetcp(void);

extern int h_errno;

static int fd=-1;
extern int f_flags;

/* opentcp() was rewritten by Martin Junius */

int opentcp(name)
char *name;
{
	struct servent *se;
	struct hostent *he;
	int a1,a2,a3,a4;
	char *errmsg;
	char *portname;
	int fd;
	short portnum;
	struct sockaddr_in server;

	debug(18,"try open tcp connection to %s",S(name));

	server.sin_family=AF_INET;

	if ((portname=strchr(name,':')))
	{
		*portname++='\0';
		if ((portnum=atoi(portname)))
			server.sin_port=htons(portnum);
		else if ((se=getservbyname(portname,"tcp")))
			server.sin_port=se->s_port;
		else server.sin_port=htons(FIDOPORT);
	}
	else
	{
		if ((se=getservbyname("fido","tcp")))
			server.sin_port=se->s_port;
		else server.sin_port=htons(FIDOPORT);
	}

	if (sscanf(name,"%d.%d.%d.%d",&a1,&a2,&a3,&a4) == 4)
		server.sin_addr.s_addr=inet_addr(name);
	else if ((he=gethostbyname(name)))
		memcpy(&server.sin_addr,he->h_addr,he->h_length);
	else
	{
		switch (h_errno)
		{
		case HOST_NOT_FOUND:	errmsg="Authoritative: Host not found"; break;
		case TRY_AGAIN:		errmsg="Non-Authoritive: Host not found"; break;
		case NO_RECOVERY:	errmsg="Non recoverable errors"; break;
		default:		errmsg="Unknown error"; break;
		}
		loginf("no IP address for %s: %s\n",name,errmsg);
		return -1;
	}

	debug(18,"trying %s at port %d",
		inet_ntoa(server.sin_addr),(int)ntohs(server.sin_port));

	signal(SIGPIPE,linedrop);
	fflush(stdin);
	fflush(stdout);
	setbuf(stdin,NULL);
	setbuf(stdout,NULL);
	close(0);
	close(1);
	if ((fd=socket(AF_INET,SOCK_STREAM,0)) != 0)
	{
		logerr("$cannot create socket (got %d, expected 0");
		open("/dev/null",O_RDONLY);
		open("/dev/null",O_WRONLY);
		return -1;
	}
	if (dup(fd) != 1)
	{
		logerr("$cannot dup socket");
		open("/dev/null",O_WRONLY);
		return -1;
	}
	clearerr(stdin);
	clearerr(stdout);
	if (connect(fd,(struct sockaddr *)&server,sizeof(server)) == -1)
	{
		loginf("$cannot connect %s",inet_ntoa(server.sin_addr));
		return -1;
	}

	f_flags=0;

	loginf("established TCP connection with %s",inet_ntoa(server.sin_addr));
	return 0;
}

void closetcp(void)
{
	shutdown(fd,2);
	signal(SIGPIPE,SIG_DFL);
}

#endif

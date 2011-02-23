#ifdef HAS_TERM

#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <netdb.h>
#include <client.h>
#include "lutil.h"
#include <sys/socket.h>
#define FIDOPORT 60179		/* my birthday */

extern void linedrop(int);

int opentcp(char*);
void closetcp(void);

static int fd=-1;

int opentcp(name)
char *name;
{

	debug(2,"try open term connection to %s",S(name));
	signal(SIGPIPE,linedrop);
	fflush(stdin);
	fflush(stdout);
	setbuf(stdin,NULL);
	setbuf(stdout,NULL);
	close(0);
	close(1);
	
	debug(2,"try to connect to TERM server");
	if ((fd = connect_server(0)) < 0)
	{
		logerr("$cannot connect to TERM server");
		return -1;
	}

	debug(2,"trying %s at port %d",
		name, FIDOPORT);

	if ((send_command(fd,C_PORT,0,"%s:%d",name,FIDOPORT)) < 0)
	{
		logerr("$cannot create termsocket (got %d, expected 0");
		open("/dev/null",O_RDONLY);
		open("/dev/null",O_WRONLY);
		return -1;
	}
	
	send_command(fd,C_DUMB, 1,0);

	if (dup(fd) != 1)
	{
		logerr("$cannot dup socket");
		open("/dev/null",O_WRONLY);
		return -1;
	}
	clearerr(stdin);
	clearerr(stdout);
	loginf("connected to %s",name);
	return 0;
}

void closetcp(void)
{
	shutdown(fd,2);
	signal(SIGPIPE,SIG_DFL);
}

#endif

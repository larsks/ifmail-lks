/*
  $Header
  
  $Log: iftelnetd.c,v $
 * Revision 1.2  1995/09/09  11:33:37  lord
 * Copyright notise
 *
 * Revision 1.1  1995/09/09  11:08:50  lord
 * Initial revision
 *
  */


/*
  Simple proxy daemon for ifcico to work with SIO/VMODEM.
  ========================================================
  Written by:  Vadim Zaliva, lord@crocodile.kiev.ua, 2:463/80

  This software is provided ``as is'' without express or implied warranty.
  Feel free to distribute it.
  
  Feel free to contact me with improovments request
  and bug reports.
  
  Some parts of this code are taken from
  1. serge terekhov, 2:5000/13@fidonet path for ifmail
  2. Vadim Kurland, vadim@gu.kiev.ua transl daemon.
  Thanks them.
  */
  
#include <unistd.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <syslog.h>
#include <string.h>
#include <getopt.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define MBUFSIZ   8192 
#define TELNETD   LOG_DEBUG
#define VERSION_S "iftelnetd v1.0B by lord@crocodile.kiev.ua" 
#define TIMEOUT   3600

#define nil NULL

extern  int  init_telnet(void);
extern  void answer (int tag, int opt  );
extern  int  read0  (char *buf, int len );
extern  int  write1 (char *buf, int len);
extern  void com_gw (int in);

#define WILL		      251
#define WONT		      252
#define DO		      253
#define DONT		      254
#define IAC		      255

#define TN_TRANSMIT_BINARY	0
#define	TN_ECHO			1
#define TN_SUPPRESS_GA		3

static char telbuf[4];
static int tellen;

int debug;

int main(int ac,char **av)
{
    struct sockaddr_in peeraddr;
    int addrlen=sizeof(struct sockaddr_in);
    char *remote_name=nil;
    char *remote_port=nil;
    int  c;
    int  s; /* socket to remote*/
    struct hostent *hp;
    struct servent *sp;
    struct sockaddr_in server;
    FILE   *f;
    time_t now;
    char *log_file=nil;
    char *tmp=nil;

    syslog(LOG_INFO,"iftelnetd: Starting...");

    debug=0;
    
    while ((c = getopt(ac,av,"dl:h:p:")) != EOF)
        switch (c) {
            case 'd':
                debug=1;
                break;

            case 'l':
		log_file=strdup(optarg);
                break;

            case 'h':
		remote_name=strdup(optarg);
                break;

            case 'p':
		remote_port=strdup(optarg);
                break;

	default:
	    ;
		syslog(LOG_ERR,"iftelnetd: Wrong number of args!");
		syslog(LOG_ERR,"iftelnetd: Usage:");
		syslog(LOG_ERR,"iftelnetd: iftelnetd [-h remote_addr] [-p remote_port] [-l logfile] [-d]");
		syslog(LOG_ERR,"iftelnetd: Aborting.");
		if(log_file)    free(log_file);
		if(remote_name) free(remote_name);
		if(remote_port) free(remote_port);
		return 1;
		break;
	}
    
    if(!remote_name)
    {
	syslog(LOG_WARNING,"iftelnetd: Remote addr not set. Assuming 'localhost'");
	remote_name=strdup("localhost");
    }
    
    if(!remote_port)
    {
	syslog(LOG_WARNING,"iftelnetd: Remote port not set. Assuming 'fido'");
	remote_port=strdup("fido");
    }

    if(getpeername(0,(struct sockaddr*)&peeraddr,&addrlen) == 0)
    {
	tmp=strdup(inet_ntoa(peeraddr.sin_addr));
	syslog(LOG_INFO,"iftelnetd: incoming TCP connection from %s",
	       tmp ? tmp : "Unknown"
	);
	syslog(LOG_INFO,"iftelnetd: Rerouting to %s:%s",
	       remote_name,
	       remote_port);
    }

    if(log_file)
    {
	now=time(nil);
	if((f=fopen(log_file,"a"))!=nil)
	{
	    fprintf(f,"%s\t%s\n",
		    ctime(&now),
		    tmp ? tmp : "Unknown"
	    );
	    fclose(f);
	}
	free(log_file);
    }

    if(tmp) free(tmp);
    
    if((sp=getservbyname(remote_port,"tcp"))==NULL) 
    {
	syslog(LOG_ERR,"iftelnetd: Can't find service: %s",remote_port);
	syslog(LOG_ERR,"iftelnetd: Aborting.");
	free(remote_name);
	free(remote_port);
	return 1;
    }

    if((s=socket(AF_INET,SOCK_STREAM,0))==-1)
    {
	syslog(LOG_ERR,"iftelnetd: Can't create Internet domain socket");
	syslog(LOG_ERR,"iftelnetd: Aborting.");
	free(remote_name);
	free(remote_port);
	return  1;
    }

    if((hp=gethostbyname(remote_name))==NULL)
    {
	syslog(LOG_ERR,"iftelnetd: %s - Unknown host",remote_name);
	syslog(LOG_ERR,"iftelnetd: Aborting.");
	free(remote_name);
	free(remote_port);
	return;
    }
  
    memset(&server,0,sizeof(server));
    memcpy((char *)&server.sin_addr,hp->h_addr,hp->h_length);
    
    server.sin_family=hp->h_addrtype;
    server.sin_port  = sp->s_port;
    
    if(connect(s,(struct sockaddr *)&server,sizeof(server)) == -1)
    {
	syslog(LOG_ERR, "iftelnetd: Can't connect %s",remote_name);
	syslog(LOG_ERR, "iftelnetd: Aborting.");
	free(remote_name);
	free(remote_port);
	return;
    }
    
    init_telnet();
    write1(VERSION_S "\r\n" ,strlen(VERSION_S)+2);

    com_gw(s);
    
    free(remote_name);
    free(remote_port);
    close(s);
    syslog(LOG_INFO,"iftelnetd: Done.");
}

/* --- This is an artwork of serge terekhov, 2:5000/13@fidonet :) --- */

void answer (int tag, int opt)
{
    char buf[3];

    if(debug)
    {
	char *r = "???";
	
	switch (tag)
	{
	case WILL:
	    r = "WILL";
	    break;
	case WONT:
	    r = "WONT";
	    break;
	case DO:
	    r = "DO";
	    break;
	case DONT:
	    r = "DONT";
	    break;
	}
	syslog(LOG_SYSLOG, "iftelnetd: TELNET send %s %d", r, opt);
    }
    buf[0] = IAC;
    buf[1] = tag;
    buf[2] = opt;
    if (write (1, buf, 3) != 3)
	syslog(LOG_ERR,"iftelnetd: $answer cant send");
}

int init_telnet(void)
{
    tellen = 0;
    answer (DO, TN_SUPPRESS_GA);
    answer (WILL, TN_SUPPRESS_GA);
    answer (DO, TN_TRANSMIT_BINARY);
    answer (WILL, TN_TRANSMIT_BINARY);
    answer (DO, TN_ECHO);
    answer (WILL, TN_ECHO);
    return 1;
}

int read0 (char *buf, int len)
{
    int n = 0, m;
    char *q, *p;
    
/*	return read (0, buf, BUFSIZ); */

    while ((n == 0) && (n = read (0, buf + tellen, MBUFSIZ - tellen)) > 0)
    {
	if (tellen)
	{
	    memcpy (buf, telbuf, tellen);
	    n += tellen;
	    tellen = 0;
	}
	if (memchr (buf, IAC, n))
	{
	    for (p = q = buf; n--; )
		if ((m = (unsigned char)*q++) != IAC)
		    *p++ = m;
		else
		{
		    if (n < 2)
		    {
			memcpy (telbuf, q - 1, tellen = n + 1);
			break;
		    }
		    --n;
		    switch (m = (unsigned char)*q++)
		    {
		    case WILL:
			m = (unsigned char)*q++; --n;
			if(debug)
			    syslog (TELNETD, "iftelnetd: TELNET: recv WILL %d", m);
			
			if (m != TN_TRANSMIT_BINARY && m != TN_SUPPRESS_GA &&
			    m != TN_ECHO)
			    answer (DONT, m);
			break;
		    case WONT:
			m = *q++; --n;
			if(debug)
			    syslog (TELNETD, "iftelnetd: TELNET: recv WONT %d", m);
			break;
		    case DO:
			m = (unsigned char)*q++; --n;
			if(debug)
			    syslog (TELNETD, "iftelnetd: TELNET: recv DO %d", m);
			if (m != TN_TRANSMIT_BINARY && m != TN_SUPPRESS_GA &&
			    m != TN_ECHO)
			    answer (WONT, m);
			break;
		    case DONT:
			m = (unsigned char)*q++; --n;
			if(debug)
			    syslog (TELNETD, "iftelnetd: TELNET: recv DONT %d", m);
			break;
		    case IAC:
			*p++ = IAC;
			break;
		    default:
			if(debug)
			    syslog (TELNETD, "iftelnetd: TELNET: recv IAC %d", m);
			break;
		    }
		}
	    n = p - buf;
	}
    }

    return n;
}

int write1 (char *buf, int len)
{
    char *q;
    int k, l;
    
    l = len;
    while ((len > 0) && (q = memchr(buf, IAC, len)))
    {
	k = (q - buf) + 1;
	if ((write (1, buf, k) != k) ||
	    (write (1, q, 1) != 1))
	    return -1;
	buf += k;
	len -= k;
    }
    if ((len > 0) && write (1, buf, len) != len)
	return -1;
    return l;
}

void com_gw(int in)
{
    fd_set               fds;
    int                  n, fdsbits;
    static struct timeval    tout = { TIMEOUT, 0 };
    unsigned char buf[MBUFSIZ];
    
    alarm(0);
    
    fdsbits = in + 1;
    
    while (1)
    {
        FD_ZERO(&    fds);
        FD_SET (in, &fds);
        FD_SET (0 , &fds);
        FD_SET (1 , &fds);
	
        tout.tv_sec  = TIMEOUT;
        tout.tv_usec = 0;
        if ((n = select(fdsbits, &fds, NULL, NULL, &tout)) > 0)
	{
            if (FD_ISSET(in, &fds))
	    {
                if ((n = read(in, buf, sizeof buf)) > 0)
		{
                    if (write1(buf, n) < 0) goto bad;
                }
		else
		    goto bad;
            }
            if (FD_ISSET(0, &fds))
	    {
                if ((n = read0(buf, sizeof buf)) > 0)
		{
                    if (write(in, buf, n) < 0) goto bad;
                }
		else goto bad;
            }
        } 
        else goto bad;
    }
bad:    ;   
}







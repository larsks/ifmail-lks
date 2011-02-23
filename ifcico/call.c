#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "lutil.h"
#include "xutil.h"
#include "ftn.h"
#include "nodelist.h"
#include "config.h"
#include "session.h"
#include "callstat.h"

extern int forcedcalls;
extern char *forcedphone;
extern char *forcedline;
#if defined(HAS_TCP) || defined(HAS_TERM)
extern char *inetaddr;
#endif

extern char *get_modem_string(modem_string*,node*);
extern int openport(char*,int);
#if defined(HAS_TCP) || defined(HAS_TERM)
extern int opentcp(char*);
extern void closetcp(void);
#endif
extern void localport(void);
extern void nolocalport(void);
extern void closeport(void);
extern int dialphone(node*,char*);
extern int hangup(node*);
extern void rdoptions(node*);
extern int nodelock(faddr*);
extern int nodeulock(faddr*);

int checkretry(st)
callstat *st;
{
	return 0;
	/* check retries and time; rc=1 - not reached, rc=2 - undialable */
}

int call(addr)
faddr *addr;
{
	node *nlent;
	int rc=1;
	int speed;
	char *portlist,*p,*q;
	callstat *st;

	if ((nlent=getnlent(addr)) == NULL)
	{
		logerr("cannot call %s: fatal in nodelist lookup",
			ascfnode(addr,0x1f));
		putstatus(addr,0,6);
		return 6;
	}

	st=getstatus(addr);
	if (!forcedcalls) {
		if ((rc=checkretry(st))) {
			loginf("cannot call %s: %s",
				ascfnode(addr,0x1f),
				(rc==1)?"retry time not reached"
				:"node undialable");
			return 5;
		}
		if (dialdelay) sleep(dialdelay);
	}

	if (nodelock(addr))
	{
		loginf("system %s locked",ascinode(addr,0x1f));
		putstatus(addr,0,4);
		return 4;
	}

	rdoptions(nlent);

	inbound=protinbound; /* master sessions are secure */

	if ((nlent->phone || forcedphone
#if defined(HAS_TCP) || defined(HAS_TERM)
					 || inetaddr
#endif
							) &&
	    (forcedcalls || 
	     (((nlent->pflag & (NL_DUMMY|NL_DOWN|NL_HOLD|NL_PVT)) == 0) && 
	      ((localoptions & NOCALL) == 0))))
	{
		loginf("calling %s (%s, phone %s)",ascfnode(addr,0x1f),
			nlent->name,nlent->phone?nlent->phone:forcedphone);
#if defined(HAS_TCP) || defined(HAS_TERM)
		if (inetaddr)
		{
			rc=opentcp(inetaddr);
			if (rc)
			{
				loginf("cannot connect %s",inetaddr);
				nodeulock(addr);
				putstatus(addr,1,2);
				return 2;
			}
		}
		else
#endif
		     if (forcedline)
		{
			p=forcedline;
			if ((q=strchr(p,':')))
			{
				*q++='\0';
				if ((*q == 'l') || (*q == 'L'))
					speed=atoi(++q);
				else
				{
					speed=atoi(q);
					if (nlent->speed < speed)
						speed=nlent->speed;
				}
			}
			else speed=0;
			rc=openport(p,speed);
			if (rc)
			{
				loginf("cannot open port %s",p);
				nodeulock(addr);
				putstatus(addr,0,1);
				return 1;
			}
		}
		else
		{
			if ((portlist=xstrcpy(get_modem_string(modemport,nlent))) == NULL)
			{
				logerr("no matching ports defined");
				nodeulock(addr);
				putstatus(addr,0,9);
				return 9;
			}
			for (rc=1,p=strtok(portlist," \t,");
				rc && p;
				p=strtok(NULL," \t,"))
			{
				if ((q=strrchr(p,':')))
				{
					*q++='\0';
					if ((*q == 'l') || (*q == 'L'))
					{
						speed=atoi(++q);
					}
					else
					{
						speed=atoi(q);
						if (nlent->speed < speed)
							speed=nlent->speed;
					}
				}
				else speed=0;
				rc=openport(p,speed);
			}
			if (rc)
			{
				loginf("no free matching ports");
				free(portlist);
				nodeulock(addr);
				putstatus(addr,0,1);
				return 1;
			}
			free(portlist);
		}
#if defined(HAS_TCP) || defined(HAS_TERM)
		if (!inetaddr)
#endif
		if ((rc=dialphone(nlent,forcedphone?forcedphone:nlent->phone)))
		{
			loginf("dial failed");
			nodeulock(addr);
			rc+=1; /* rc=2 - dial fail, rc=3 - could not reset */
		}

		if (rc == 0)
		{
#if defined(HAS_TCP) || defined(HAS_TERM)
			if (!inetaddr)
#endif
				nolocalport();
			rc=session(addr,nlent,SESSION_MASTER,
				SESSION_UNKNOWN,NULL);
			if (rc) rc=abs(rc)+10;
		}
#if defined(HAS_TCP) || defined(HAS_TERM)
		if (inetaddr)
		{
			closetcp();
		}
		else
#endif
		{
			localport();
			hangup(nlent);
			closeport();
		}
	}
	else
	{
		loginf("cannot call %s (%s, phone %s)",
			ascfnode(addr,0x1f),S(nlent->name),
			S(nlent->phone));
		if ((nlent->phone || forcedphone
#if defined(HAS_TCP) || defined(HAS_TERM)
						 || inetaddr
#endif
								))
			rc=8;
		else rc=7;
		nodeulock(addr);
	}

	if ((rc == 2) || (rc == 30))
		putstatus(addr,1,rc);
	else
		putstatus(addr,0,rc);
	return rc;
}

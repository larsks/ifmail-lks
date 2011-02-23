#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include "lutil.h"
#include "nodelist.h"
#include "config.h"

int chat(node*,modem_string*,modem_string*,modem_string*,int,char*);

int dialphone(nlent,phone)
node *nlent;
char *phone;
{
	int rc;

	debug(18,"dialing %s",S(phone));
	if ((rc=chat(nlent,modemreset,modemok,modemerror,timeoutreset,phone)))
	{
		logerr("could not reset the modem");
		return 2;
	}
	else if ((rc=chat(nlent,modemdial,modemconnect,modemerror,
				timeoutconnect,phone)))
	{
		loginf("could not connect to the remote");
		return 1;
	}
	else return 0;
}

int hangup(nlent)
node *nlent;
{
	debug(18,"hanging up");
	chat(nlent,modemhangup,NULL,NULL,0,NULL);
	sleep(1);
	return 0;
}

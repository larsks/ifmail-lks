#include <stdio.h>
#include <string.h>
#include "lutil.h"
#include "ftn.h"
#include "nodelist.h"
#include "session.h"
#include "config.h"

extern int rawport(void);
extern void nolocalport(void);
extern int cookedport(void);
extern void rdoptions(node *);

int answer(stype)
char *stype;
{
	int st,rc;
	node *nlent;

	if ((nlent=getnlent(NULL)) == NULL)
	{
		logerr("could not get dummy nodelist entry");
		return 1;
	}

	rdoptions(nlent);

	inbound=norminbound; /* slave session is unsecure by default */

	if (stype == NULL)
		st=SESSION_UNKNOWN;
	else if (strcmp(stype,"tsync") == 0)
		st=SESSION_FTSC;
	else if (strcmp(stype,"yoohoo") == 0)
		st=SESSION_YOOHOO;
	else if (strncmp(stype,"**EMSI_",7) == 0)
		st=SESSION_EMSI;
	else
		st=SESSION_UNKNOWN;
	debug(10,"answer to \"%s\" (%d) call",stype?stype:"unknown type",st);

	if ((rc=rawport()) != 0)
		logerr("unable to set raw mode");
	else
	{
		nolocalport();
		rc=session(NULL,NULL,SESSION_SLAVE,st,stype);
	}

	cookedport();
	return rc;
}

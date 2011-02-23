/*
	contributed by Stanislav Voronyi <stas@uanet.kharkov.ua>
*/

#ifdef HAS_TCP

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "lutil.h"
#include "ttyio.h"
#include "session.h"
#include "statetbl.h"
#include "config.h"
#include "emsi.h"

extern int made_request;
extern int tcprcvfiles(void);
extern int tcpsndfiles(file_list *);
extern file_list *respond_wazoo(char *);

extern int rxtcp(void)
{
	int rc=0;
	fa_list *eff_remote,tmpl;
	file_list *tosend=NULL,**tmpfl;

	debug(10,"start rxtcp transfer");
	if (emsi_remote_lcodes & NPU)
	{
		loginf("remote requested \"no pickup\", no send");
		eff_remote=NULL;
	}
	else if (emsi_remote_lcodes & PUP)
	{
		loginf("remote requested \"pickup primary\"");
		tmpl.addr=remote->addr;
		tmpl.next=NULL;
		eff_remote=&tmpl;
	}
	else eff_remote=remote;
	tosend=create_filelist(eff_remote,ALL_MAIL,0);

	if ((rc=tcprcvfiles()) == 0)
	{
		if ((emsi_local_opts & NRQ) == 0)
		{
			for (tmpfl=&tosend;*tmpfl;tmpfl=&((*tmpfl)->next));
			*tmpfl=respond_wazoo(NULL);
		}

		if ((tosend != NULL) || ((emsi_remote_lcodes & NPU) == 0))
			rc=tcpsndfiles(tosend);

		if ((rc == 0) && (made_request))
		{
			loginf("freq was made, trying to receive files");
			rc=tcprcvfiles();
		}
	}

	tidy_filelist(tosend,(rc == 0));
	debug(10,"rxtcp transfer rc=%d",rc);
	return rc;
}

extern int txtcp(void)
{
	int rc=0;
	file_list *tosend=NULL,*respond=NULL;
	char *nonhold_mail;

	debug(10,"start txtcp transfer");
	if (localoptions & NOHOLD) nonhold_mail=ALL_MAIL;
	else nonhold_mail=NONHOLD_MAIL;
	if (emsi_remote_lcodes & HAT)
	{
		loginf("remote asked to \"hold all traffic\", no send");
		tosend=NULL;
	}
	else tosend=create_filelist(remote,nonhold_mail,0);

	if ((tosend != NULL) || ((emsi_remote_lcodes & NPU) == 0))
		rc=tcpsndfiles(tosend);
	if (rc == 0)
		if ((rc=tcprcvfiles()) == 0)
			if ((emsi_local_opts & NRQ) == 0)
				if ((respond=respond_wazoo(NULL)))
					rc=tcpsndfiles(respond);

	tidy_filelist(tosend,(rc == 0));
	tidy_filelist(respond,0);
	debug(10,"end txtcp transfer");
	return rc;
}
#endif /* HAS_TCP */

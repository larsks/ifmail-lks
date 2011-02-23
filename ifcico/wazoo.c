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
extern int zmsndfiles(file_list*);
extern int zmrcvfiles(void);
extern file_list *respond_wazoo(char*);

extern int rxwazoo(void);
extern int rxwazoo(void)
{
	int rc=0;
	fa_list *eff_remote,tmpl;
	file_list *tosend=NULL,**tmpfl;

	debug(10,"start rxwazoo transfer");
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

	if ((rc=zmrcvfiles()) == 0)
	{
		if ((emsi_local_opts & NRQ) == 0)
		{
			for (tmpfl=&tosend;*tmpfl;tmpfl=&((*tmpfl)->next));
			*tmpfl=respond_wazoo(NULL);
		}

		if ((tosend != NULL) || ((emsi_remote_lcodes & NPU) == 0))
			rc=zmsndfiles(tosend);

		if ((rc == 0) && (made_request))
		{
			loginf("freq was made, trying to receive files");
			rc=zmrcvfiles();
		}
	}

	tidy_filelist(tosend,(rc == 0));
	debug(10,"rxwazoo transfer rc=%d",rc);
	return rc;
}

extern int txwazoo(void);
extern int txwazoo(void)
{
	int rc=0;
	file_list *tosend=NULL,*respond=NULL;
	char *nonhold_mail;

	debug(10,"start txwazoo transfer");
	if (localoptions & NOHOLD) nonhold_mail=ALL_MAIL;
	else nonhold_mail=NONHOLD_MAIL;
	if (emsi_remote_lcodes & HAT)
	{
		loginf("remote asked to \"hold all traffic\", no send");
		tosend=NULL;
	}
	else tosend=create_filelist(remote,nonhold_mail,0);

	if ((tosend != NULL) || ((emsi_remote_lcodes & NPU) == 0))
		rc=zmsndfiles(tosend);
	if (rc == 0)
		if ((rc=zmrcvfiles()) == 0)
			if ((emsi_local_opts & NRQ) == 0)
				if ((respond=respond_wazoo(NULL)))
					rc=zmsndfiles(respond);

	tidy_filelist(tosend,(rc == 0));
	tidy_filelist(respond,0);
	debug(10,"end txwazoo transfer");
	return rc;
}

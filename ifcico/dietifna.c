#include <stdlib.h>
#include "lutil.h"
#include "ttyio.h"
#include "session.h"
#include "emsi.h"

extern int made_request;
static int sendfiles(file_list*);
extern int xmsndfiles(file_list*);
static int xmrcvfiles(void);
extern int xmrecv(char*);
extern file_list *respond_wazoo(char*);

int rxdietifna(void);
int rxdietifna(void)
{
	int rc;
	file_list *tosend=NULL,**tmpfl;

	debug(10,"start rxDietIFNA");
	session_flags |= SESSION_IFNA;
	session_flags &= ~SESSION_BARK;
	tosend=create_filelist(remote,ALL_MAIL,0);
	if ((rc=xmrcvfiles()) == 0)
	{
		if ((emsi_local_opts & NRQ) == 0)
		{
			for (tmpfl=&tosend;*tmpfl;tmpfl=&((*tmpfl)->next));
			*tmpfl=respond_wazoo(NULL);
		}
		rc=sendfiles(tosend);
		/* we are not sending file requests in slave session */
	}
	tidy_filelist(tosend,(rc == 0));
	debug(10,"rxdietifna transfer rc=%d",rc);
	return rc;
}

int txdietifna(void);
int txdietifna(void)
{
	int rc;
	file_list *tosend=NULL,*respond=NULL;
	char *nonhold_mail;

	debug(10,"start txDietIFNA");
	session_flags |= SESSION_IFNA;
	session_flags &= ~SESSION_BARK;
	if (localoptions & NOHOLD) nonhold_mail=ALL_MAIL;
	else nonhold_mail=NONHOLD_MAIL;
	tosend=create_filelist(remote,nonhold_mail,2);

	if ((rc=sendfiles(tosend)) == 0)
		if ((rc=xmrcvfiles()) == 0)
			if ((emsi_local_opts & NRQ) == 0)
				if ((respond=respond_wazoo(NULL)))
					rc=sendfiles(respond);
	/* but we are trying to respond other's file requests in master */
	/* session, though they are not allowed by the DietIFNA protocol */
	
	tidy_filelist(tosend,(rc == 0));
	tidy_filelist(respond,0);
	debug(10,"rxdietifna transfer rc=%d",rc);
	return rc;
}

int xmrcvfiles(void)
{
	int rc;

	while ((rc=xmrecv(NULL)) == 0);
	if (rc == 1) return 0;
	else return rc;
}

int sendfiles(tosend)
file_list *tosend;
{
	int c,count;

	count=0;
	while (((c=GETCHAR(15)) >= 0) && (c != NAK) && (c != 'C') && 
	       (count++ < 9)) 
		debug(11,"got '%s' waiting for C/NAK",
			printablec(c));
	if (c == NAK)
	{
		session_flags &= ~FTSC_XMODEM_CRC;
	}
	else if (c == 'C')
	{
		session_flags |= FTSC_XMODEM_CRC;
	}
	else if (c < 0) return c;
	else return 1;

	return xmsndfiles(tosend);
}

#include <stdio.h>
#include "lutil.h"
#include "ttyio.h"
#include "session.h"
#include "statetbl.h"

int recvbark(void);

static int recv_bark(void);
extern unsigned INT16 crc16(char*,int);
extern int xmsndfiles(file_list*);
extern file_list *respond_bark(char*);

int recvbark(void)
{
	if ((session_flags&SESSION_BARK) && !(localoptions&NOFREQS))
	{
		return recv_bark();
	}
	else /* deny requests */
	{
		PUTCHAR(CAN);
		return STATUS;
	}
}

SM_DECL(recv_bark,"recvbark")
SM_STATES
	sendenq,waitack,waitchar,scanreq,sendack,
	waitnak,sendfiles
SM_NAMES
	"sendenq","waitack","waitchar","scanreq","sendack",
	"waitnak","sendfiles"
SM_EDECL

	int c,c1,c2;
	INT16 lcrc,rcrc;
	char buf[256],*p=NULL;
	int count=0,rc=0;
	file_list *tosend=NULL;

SM_START(sendenq)

SM_STATE(sendenq)

	if (count++ > 6)
	{
		loginf("failed to get bark request");
		SM_ERROR;
	}

	PUTCHAR(ENQ);
	if (STATUS) {SM_ERROR;}
	else {SM_PROCEED(waitack);}

SM_STATE(waitack)

	c=GETCHAR(15);
	if (c == TIMEOUT) 
	{
		debug(10,"recvbark got timeout waiting for ACK");
		SM_PROCEED(sendenq);
	}
	else if (c < 0)
	{
		SM_ERROR;
	}
	else switch (c)
	{
	case ACK:	p=buf; SM_PROCEED(waitchar); break;
	case ETB:	SM_SUCCESS; break;
	case ENQ:	PUTCHAR(ETB); SM_PROCEED(waitack); break;
	case EOT:	PUTCHAR(ACK); SM_PROCEED(waitack); break;
	default:	debug(10,"recvbark got '%s' waiting for ACK",
				printablec(c));
			SM_PROCEED(waitack); break;
	}

SM_STATE(waitchar)

	c=GETCHAR(15);
	if (c == TIMEOUT) 
	{
		debug(10,"recvbark got timeout waiting for char");
		SM_PROCEED(sendenq);
	}
	else if (c < 0)
	{
		SM_ERROR;
	}
	else switch (c)
	{
	case ACK:	SM_PROCEED(waitchar); break;
	case ETX:	*p='\0'; SM_PROCEED(scanreq); break;
	case SUB:	SM_PROCEED(sendenq); break;
	default:	if ((p-buf) < sizeof(buf)) *p++=c;
			SM_PROCEED(waitchar); break;
	}

SM_STATE(scanreq)

	lcrc=crc16(buf,strlen(buf));
	c1=GETCHAR(15);
	if (c1 == TIMEOUT) {SM_PROCEED(sendenq);}
	else if (c1 < 0) {SM_ERROR;}
	c2=GETCHAR(15);
	if (c2 == TIMEOUT) {SM_PROCEED(sendenq);}
	else if (c2 < 0) {SM_ERROR;}
	rcrc=(c2<<8)+(c1&0xff);
	if (lcrc != rcrc)
	{
		debug(10,"lcrc 0x%04x != rcrc 0x%04x",lcrc,rcrc);
		SM_PROCEED(sendenq);
	}
	SM_PROCEED(sendack);

SM_STATE(sendack)

	count=0;
	PUTCHAR(ACK);
	tosend=respond_bark(buf);
	SM_PROCEED(waitnak);

SM_STATE(waitnak)

	c=GETCHAR(15);
	if (c == TIMEOUT) 
	{
		debug(10,"recvbark got timeout waiting for NAK");
		SM_PROCEED(sendenq);
	}
	else if (c < 0)
	{
		SM_ERROR;
	}
	else switch (c)
	{
	case NAK:	session_flags &= ~FTSC_XMODEM_CRC; /* fallthrough */
	case 'C':	session_flags |= FTSC_XMODEM_CRC;
			SM_PROCEED(sendfiles); break;
	case ENQ:	PUTCHAR(ETB); SM_PROCEED(waitack); break;
	case SUB:	SM_PROCEED(sendenq); break;
	default:	debug(10,"recvbark got '%s' waiting for NAK",
				printablec(c));
			SM_PROCEED(waitack); break;
	}

SM_STATE(sendfiles)

	rc=xmsndfiles(tosend);
	tidy_filelist(tosend,0);
	if (rc == 0) {SM_PROCEED(sendenq);}
	else {SM_ERROR;}

SM_END
SM_RETURN

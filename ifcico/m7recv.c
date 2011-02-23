#include <ctype.h>
#include "statetbl.h"
#include "ttyio.h"
#include "lutil.h"

static int m7_recv(void);
static char* fn;
static int last;

int m7recv(char*);
int m7recv(fname)
char *fname;
{
	int rc;

	fn=fname;
	last=0;
	rc=m7_recv();
	if (rc) return -1;
	else if (last) return 1;
	else return 0;
}

SM_DECL(m7_recv,"m7recv")
SM_STATES
	sendnak,waitack,waitchar,sendack,sendcheck,waitckok
SM_NAMES
	"sendnak","waitack","waitchar","sendack","sendcheck","waitckok"
SM_EDECL

	int count=0;
	int c,i=0;
	char *p=fn;
	char cs=0;

SM_START(waitchar)

SM_STATE(sendnak)

	if (count++ > 6)
	{
		loginf("too many tries getting modem7 name");
		SM_ERROR;
	}
	p=fn;
	cs=SUB;
	i=0;
	PUTCHAR(NAK);
	if (STATUS) {SM_ERROR;}
	else {SM_PROCEED(waitack);}

SM_STATE(waitack)

	c=GETCHAR(15);
	if (c == TIMEOUT)
	{
		debug(11,"m7 got timout waiting for ACK");
		SM_PROCEED(sendnak);
	}
	else if (c < 0)
	{
		SM_ERROR;
	}
	else switch (c)
	{
	case ACK:	SM_PROCEED(waitchar); break;
	case EOT:	last=1; SM_SUCCESS; break;
	default:	debug(11,"m7 got '%s' waiting for ACK",
				printablec(c));
			break;
	}

SM_STATE(waitchar)

	c=GETCHAR(15);
	if (c == TIMEOUT)
	{
		debug(11,"m7 got timeout waiting for char",c);
		SM_PROCEED(sendnak);
	}
	else if (c < 0)
	{
		SM_ERROR;
	}
	else switch (c)
	{
	case EOT:	last=1; SM_SUCCESS; break;
	case SUB:	*p='\0'; SM_PROCEED(sendcheck); break;
	case 'u':	SM_PROCEED(sendnak); break;
	default:	cs+=c;
			if (i < 15)
			{
				if (c != ' ')
				{
					if (i == 8) *p++='.';
					*p++=tolower(c);
				}
				i++;
			}
			SM_PROCEED(sendack); break;
	}

SM_STATE(sendack)

	PUTCHAR(ACK);
	SM_PROCEED(waitchar);

SM_STATE(sendcheck)

	PUTCHAR(cs);
	SM_PROCEED(waitckok);

SM_STATE(waitckok)

	c=GETCHAR(15);
	if (c == TIMEOUT)
	{
		debug(11,"m7 got timout waiting for ack ACK");
		SM_PROCEED(sendnak);
	}
	else if (c < 0)
	{
		SM_ERROR;
	}
	else if (c == ACK)
	{
		SM_SUCCESS;
	}
	else
	{
		SM_PROCEED(sendnak);
	}

SM_END
SM_RETURN

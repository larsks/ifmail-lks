#include <ctype.h>
#include <string.h>
#include "statetbl.h"
#include "ttyio.h"
#include "lutil.h"

static int m7_send(void);

static char *fn;

int m7send(char*);
int m7send(fname)
char *fname;
{
	fn=fname;
	return m7_send();
}

SM_DECL(m7_send,"m7send")
SM_STATES
	waitnak,sendack,sendchar,waitack,sendsub,
	waitcheck,ackcheck
SM_NAMES
	"waitnak","sendack","sendchar","waitack","sendsub",
	"waitcheck","ackcheck"
SM_EDECL

	char buf[12],*p;
	int i,c,count=0;
	char cs=SUB;

	memset(buf,' ',sizeof(buf));
	for (i=0,p=fn;(i<8) && (*p) && (*p != '.');i++,p++)
		buf[i]=toupper(*p);
	if (*p == '.') p++;
	for (;(i<11) && (*p);i++,p++)
		buf[i]=toupper(*p);
	for (i=0;i<11;i++)
		cs+=buf[i];
	buf[11]='\0';
	debug(11,"modem7 filename \"%s\", checksum %02x",
		buf,(unsigned char)cs);

SM_START(sendack)

SM_STATE(waitnak)

	if (count++ > 6)
	{
		loginf("too many tries sending modem7 name");
		SM_ERROR;
	}

	c=GETCHAR(15);
	if (c == TIMEOUT)
	{
		debug(11,"m7 got timeout waiting NAK");
		SM_PROCEED(waitnak);
	}
	else if (c < 0)
	{
		SM_ERROR;
	}
	else if (c == NAK)
	{
		SM_PROCEED(sendack);
	}
	else
	{
		debug(11,"m7 got '%s' waiting NAK",
			printablec(c));
		PUTCHAR('u');
		SM_PROCEED(waitnak);
	}


SM_STATE(sendack)

	i=0;
	PUTCHAR(ACK);
	if (STATUS) {SM_ERROR;}
	else {SM_PROCEED(sendchar)};

SM_STATE(sendchar)

	if (i > 11) {SM_PROCEED(sendsub);}

	PUTCHAR(buf[i++]);
	if (STATUS) {SM_ERROR;}
	else {SM_PROCEED(waitack)};

SM_STATE(waitack)

	c=GETCHAR(15);
	if (c == TIMEOUT)
	{
		debug(11,"m7 got timeout waiting ACK for char %d",i);
		PUTCHAR('u');
		SM_PROCEED(waitnak);
	}
	else if (c < 0)
	{
		SM_ERROR;
	}
	else if (c == ACK)
	{
		SM_PROCEED(sendchar);
	}
	else
	{
		debug(11,"m7 got '%s' waiting ACK for char %d",
			printablec(c),i);
		PUTCHAR('u');
		SM_PROCEED(waitnak);
	}

SM_STATE(sendsub)

	PUTCHAR(SUB);
	SM_PROCEED(waitcheck);

SM_STATE(waitcheck)

	c=GETCHAR(15);
	if (c == TIMEOUT)
	{
		debug(11,"m7 got timeout waiting check");
		PUTCHAR('u');
		SM_PROCEED(waitnak);
	}
	else if (c < 0)
	{
		SM_ERROR;
	}
	else if (c == cs)
	{
		SM_PROCEED(ackcheck);
	}
	else
	{
		debug(11,"m7 got %02x waiting check %02x",
			(unsigned char)c,(unsigned char)cs);
		PUTCHAR('u');
		SM_PROCEED(waitnak);
	}

SM_STATE(ackcheck)

	PUTCHAR(ACK);
	if (STATUS) {SM_ERROR;}
	else {SM_SUCCESS;}

SM_END
SM_RETURN

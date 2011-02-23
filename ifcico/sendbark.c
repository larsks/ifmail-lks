#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "ftn.h"
#include "lutil.h"
#include "ttyio.h"
#include "session.h"
#include "statetbl.h"

extern unsigned INT16 crc16(char*,int);
extern char *reqname(faddr*);
extern int xmrecv(char*);

static int send_bark(void);
static char *nm,*pw,*dt;

int sendbark(void);
int sendbark(void)
{
	char *fn;
	FILE *fp;
	char buf[256],*p;
	int rc=0;

	fn=reqname(remote->addr);
	if ((fp=fopen(fn,"r")) == NULL)
	{
		debug(10,"no request file for this node");
		PUTCHAR(ETB);
		return 0;
	}
	while (fgets(buf,sizeof(buf)-1,fp))
	{
		nm=buf;
		pw=strchr(buf,'!');
		/* if ((dt=strchr(buf,'-')) == NULL) */
		dt=strchr(buf,'+');

		if (pw) *pw++='\0';
		if (dt) *dt++='\0';

		if (nm)
		{
			while (isspace(*nm)) nm++;
			for (p=nm;(*p != '!') && (*p != '+') && 
				(!isspace(*p));p++);
			*p='\0';
		}
		if (pw)
		{
			while (isspace(*pw)) pw++;
			for (p=pw;(*p != '!') && (*p != '+') && 
				 (!isspace(*p));p++);
			*p='\0';
		}
		else pw="";
		if (dt)
		{
			while (isspace(*nm)) nm++;
			for (p=nm;(*p != '!') && (*p != '+') && 
				(*p != '-') && (!isspace(*p));p++);
			*p='\0';
		}
		else dt="0";

		if (*nm == ';') continue;

		loginf("sending bark request for \"%s\", password \"%s\", update \"%s\"",
			S(nm),S(pw),S(dt));
		if ((rc=send_bark())) break;
	}
	if (rc == 0) PUTCHAR(ETB);
	fclose(fp);
	if (rc == 0) unlink(fn);
	return rc;
}

SM_DECL(send_bark,"sendbark")
SM_STATES
	send,waitack,getfile
SM_NAMES
	"send","waitack","getfile"
SM_EDECL

	int c;
	char buf[256];
	unsigned INT16 crc;
	int count=0;

	sprintf(buf,"%s %s %s",nm,dt,pw);
	crc=crc16(buf,strlen(buf));
	debug(10,"sending bark packet \"%s\", crc=0x%04x",buf,crc);

SM_START(send)

SM_STATE(send)

	if (count++ > 6)
	{
		loginf("bark request failed");
		SM_ERROR;
	}

	PUTCHAR(ACK);
	PUT(buf,strlen(buf));
	PUTCHAR(ETX);
	PUTCHAR(crc&0xff);
	PUTCHAR((crc>>8)&0xff);
	if (STATUS) {SM_ERROR;}
	else {SM_PROCEED(waitack);}

SM_STATE(waitack)

	c=GETCHAR(15);
	if (c == TIMEOUT)
	{
		debug(10,"sendbark got timeout waiting for ACK");
		SM_PROCEED(send);
	}
	else if (c < 0)
	{
		SM_PROCEED(send);
	}
	else if (c == ACK)
	{
		SM_PROCEED(getfile);
	}
	else if (c == NAK)
	{
		SM_PROCEED(send);
	}
	else
	{
		debug(10,"sendbark got %s waiting for ACK",
			printablec(c));
		SM_PROCEED(send);
	}

SM_STATE(getfile)

	switch (xmrecv(NULL))
	{
	case 0:	SM_PROCEED(getfile); break;
	case 1:	SM_SUCCESS; break;
	default: SM_ERROR; break;
	}

SM_END
SM_RETURN

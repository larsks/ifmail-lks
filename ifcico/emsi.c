#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "xutil.h"
#include "lutil.h"
#include "ttyio.h"
#include "session.h"
#include "statetbl.h"
#include "config.h"
#include "emsi.h"
#include "nodelist.h"
#include "version.h"

#ifdef HAS_TCP
#define LOCAL_PROTOS (ZMO | ZAP | TCP)
#else
#define LOCAL_PROTOS (ZMO | ZAP)
#endif

#ifdef HAS_TCP
extern int rxtcp(void);
extern int txtcp(void);
#endif
extern int janus(void);
extern int rxwazoo(void);
extern int txwazoo(void);
extern unsigned INT16 crc16(char*,int);
extern void rdoptions(node *);

static int rxemsi(void);
static int txemsi(void);
static char *intro;
static int caller;

int emsi_local_lcodes;
int emsi_remote_lcodes;
int emsi_local_protos;
int emsi_remote_protos;
int emsi_local_opts;
int emsi_remote_opts;
char *emsi_local_password;
char *emsi_remote_password;
char emsi_remote_comm[4]="8N1";

int rx_emsi(data)
char *data;
{
	int rc;
	fa_list *tmp,*tmr;
	int denypw=0;
	node *nlent=NULL;

	loginf("start inbound EMSI session");
	emsi_local_lcodes=0;
	if (localoptions & NOPUA) emsi_local_lcodes |= PUP;
	emsi_remote_lcodes=0;
	emsi_local_protos=LOCAL_PROTOS;
	if (localoptions & NOZMODEM) emsi_local_protos &= ~(ZMO|ZAP|DZA);
	if (localoptions & NOZEDZAP) emsi_local_protos &= ~ZAP;
	if (localoptions & NOJANUS) emsi_local_protos &= ~JAN;
	if (localoptions & NOHYDRA) emsi_local_protos &= ~HYD;
#ifdef HAS_TCP
	if ((localoptions & NOTCP) || ((session_flags & SESSION_TCP) == 0))
	{
		emsi_local_protos &= ~TCP;
	}
#endif
	emsi_remote_protos=0;
	emsi_local_opts=XMA;
	emsi_remote_opts=0;
	emsi_local_password=NULL;
	emsi_remote_password=NULL;
	intro=data+2;
	caller=0;

	if ((rc=rxemsi())) return rc;

	if (localoptions & NOFREQS) emsi_local_opts |= NRQ;

	debug(10,"local  lcodes 0x%04x, protos 0x%04x, opts 0x%04x",
		emsi_local_lcodes,emsi_local_protos,emsi_local_opts);
	debug(10,"remote lcodes 0x%04x, protos 0x%04x, opts 0x%04x",
		emsi_remote_lcodes,emsi_remote_protos,emsi_remote_opts);

	emsi_local_protos &= emsi_remote_protos;
#ifdef HAS_TCP
	if (emsi_local_protos & TCP) emsi_local_protos &= TCP;
	else
#endif
	     if (emsi_local_protos & HYD) emsi_local_protos &= HYD;
	else if (emsi_local_protos & JAN) emsi_local_protos &= JAN;
	else if (emsi_local_protos & ZAP) emsi_local_protos &= ZAP;
	else if (emsi_local_protos & ZMO) emsi_local_protos &= ZMO;
	else if (emsi_local_protos & DZA) emsi_local_protos &= DZA;
	else if (emsi_local_protos & KER) emsi_local_protos &= KER;

	emsi_local_password=NULL;

	for (tmr=remote;tmr;tmr=tmr->next)
	if (((nlent=getnlent(tmr->addr))) &&
	    (nlent->pflag != NL_DUMMY))
	{
		loginf("remote is a listed system");
		inbound=listinbound;
		break;
	}
	if (nlent) rdoptions(nlent);

	for (tmp=pwlist;tmp;tmp=tmp->next)
	for (tmr=remote;tmr;tmr=tmr->next)
	if (metric(tmr->addr,tmp->addr) == 0)
	{
		if (strncasecmp(emsi_remote_password,tmp->addr->name,8) != 0)
		{
			denypw=1;
			loginf("remote gave password \"%s\", expected \"%s\"",
				S(emsi_remote_password),S(tmp->addr->name));
			emsi_local_password="BAD_PASSWORD";
			emsi_local_lcodes=HAT;
		}
		else
		{
			emsi_local_password=tmp->addr->name;
			inbound=protinbound;
			loginf("remote gave correct password, protected EMSI session");
		}
	}

	debug(10,"local  lcodes 0x%04x, protos 0x%04x, opts 0x%04x",
		emsi_local_lcodes,emsi_local_protos,emsi_local_opts);

	if ((rc=txemsi())) return rc;

	if (denypw || (emsi_local_protos == 0))
	{
		loginf("refusing remote: %s",
			emsi_local_protos?"bad password presented":
				"no common protocols");
		return 0;
	}
	if ((emsi_remote_opts & NRQ) == 0) session_flags |= SESSION_WAZOO;
	else session_flags &= ~SESSION_WAZOO;
#ifdef HAS_TCP
	if (emsi_local_protos & TCP) return rxtcp();
	else
#endif
	     if (emsi_local_protos & JAN) return janus();
	else return rxwazoo();
}

int tx_emsi(data)
char* data;
{
	int rc;

	loginf("start outbound EMSI session");
	emsi_local_lcodes=PUA;
	if (localoptions & NOPUA)
	{
		emsi_local_lcodes |= PUP;
		emsi_local_lcodes &= ~PUA;
	}
	emsi_remote_lcodes=0;
	emsi_local_protos=LOCAL_PROTOS;
	if (localoptions & NOZMODEM) emsi_local_protos &= ~(ZMO|ZAP|DZA);
	if (localoptions & NOZEDZAP) emsi_local_protos &= ~ZAP;
	if (localoptions & NOJANUS) emsi_local_protos &= ~JAN;
	if (localoptions & NOHYDRA) emsi_local_protos &= ~HYD;
#ifdef HAS_TCP
	if ((localoptions & NOTCP) || ((session_flags & SESSION_TCP) == 0))
	{
		emsi_local_protos &= ~TCP;
	}
#endif
	emsi_remote_protos=0;
	emsi_local_opts=XMA;
	if (localoptions & NOFREQS) emsi_local_opts |= NRQ;
	emsi_remote_opts=0;
	emsi_local_password=NULL;
	emsi_remote_password=NULL;
	intro=data+2;
	caller=1;
	emsi_local_password=NULL;

	debug(10,"local  lcodes 0x%04x, protos 0x%04x, opts 0x%04x",
		emsi_local_lcodes,emsi_local_protos,emsi_local_opts);

	if ((rc=txemsi())) return rc;
	else if ((rc=rxemsi())) return rc;

	debug(10,"remote lcodes 0x%04x, protos 0x%04x, opts 0x%04x",
		emsi_remote_lcodes,emsi_remote_protos,emsi_remote_opts);

	if ((emsi_remote_protos == 0) || (emsi_remote_lcodes & HAT))
	{
		loginf("remote refused us: %s",
			emsi_remote_protos?"traffic held":"no common protos");
		return 0;
	}
	emsi_local_protos &= emsi_remote_protos;
	if ((emsi_remote_opts & NRQ) == 0) session_flags |= SESSION_WAZOO;
	else session_flags &= ~SESSION_WAZOO;
#ifdef HAS_TCP
	if (emsi_local_protos & TCP) return txtcp();
	else 
#endif
	     if (emsi_local_protos & JAN) return janus();
	else return txwazoo();
}

SM_DECL(rxemsi,"rxemsi")
SM_STATES
	waitpkt,waitchar,checkemsi,getdat,checkpkt,checkdat,
	sendnak,sendack
SM_NAMES
	"waitpkt","waitchar","checkemsi","getdat","checkpkt","checkdat",
	"sendnak","sendack"
SM_EDECL

	int c=0;
	unsigned INT16 lcrc,rcrc;
	int len;
	int standby=0,tries=0;
	char buf[13],*p;
	char *databuf=NULL;

	p=buf;
	databuf=xstrcpy(intro);

SM_START(checkpkt)

SM_STATE(waitpkt)

	standby=0;
	SM_PROCEED(waitchar);

SM_STATE(waitchar)

	c=GETCHAR(5);
	if (c == TIMEOUT)
	{
		if (++tries > 9)
		{
			loginf("too many tries waiting EMSI handshake");
			SM_ERROR;
		}
		else {SM_PROCEED(sendnak);}
	}
	else if (c < 0)
	{
		SM_ERROR;
	}
	else if ((c >= ' ') && (c <= '~'))
	{
		if (c == '*')
		{
			standby=1;
			p=buf;
			*p='\0';
		}
		else if (standby)
		{
			if ((p-buf) < (sizeof(buf)-1))
			{
				*p++=c;
				*p='\0';
			}
			if ((p-buf) >= (sizeof(buf)-1))
			{
				standby=0;
				SM_PROCEED(checkemsi);
			}
		}
	}
	else switch(c)
	{
	case DC1:	break;
	case '\n':
	case '\r':	standby=0;
			break;
	default:	standby=0;
			debug(10,"got '%s' reading emsi",
				printablec(c));
			break;
	}

	SM_PROCEED(waitchar);

SM_STATE(checkemsi)

	debug(10,"rxemsi got: \"%s\"",buf);
	if (strncasecmp(buf,"EMSI_DAT",8) == 0) {SM_PROCEED(getdat);}
	else if (strncasecmp(buf,"EMSI_",5) == 0)
	{
		if (databuf) free(databuf);
		databuf=xstrcpy(buf);
		SM_PROCEED(checkpkt);
	}
	else
	{
		SM_PROCEED(waitpkt);
	}

SM_STATE(getdat)

	debug(10,"try get emsi_dat packet starting with \"%s\"",buf);

	if (sscanf(buf+8,"%04x",&len) != 1)
		{SM_PROCEED(sendnak);}

	len += 16; /* strlen("EMSI_DATxxxxyyyy"), include CRC */
	if (databuf) free(databuf);
	databuf=xmalloc(len+1);
	strcpy(databuf,buf);
	p=databuf+strlen(databuf);

	while (((p-databuf) < len) && ((c=GETCHAR(8)) >= 0))
	{
		debug(10,"got '%s'",printablec(c));
		*p++=c;
		*p='\0';
	}

	debug(10,"len %d, databuf \"%s\"",len,databuf);

	if (c == TIMEOUT)
	{
		SM_PROCEED(sendnak);
	}
	else if (c < 0)
	{
		loginf("error while reading EMSI_DAT packet");
		SM_ERROR;
	}

	SM_PROCEED(checkdat);

SM_STATE(checkpkt)

	if (strncasecmp(databuf,"EMSI_DAT",8) == 0)
	{
		SM_PROCEED(checkdat);
	}

	lcrc=crc16(databuf,8);
	sscanf(databuf+8,"%04hx",&rcrc);
	if (lcrc != rcrc)
	{
		loginf("got EMSI packet \"%s\" with bad crc: %04x/%04x",
			printable(databuf,0),lcrc,rcrc);
		SM_PROCEED(sendnak);
	}
	if (strncasecmp(databuf,"EMSI_HBT",8) == 0)
	{
		tries=0;
		SM_PROCEED(waitpkt);
	}
	else if (strncasecmp(databuf,"EMSI_INQ",8) == 0)
	{
		SM_PROCEED(sendnak);
	}
	else
	{
		debug(10,"rxemsi ignores packet \"%s\"",databuf);
		SM_PROCEED(waitpkt);
	}

SM_STATE(checkdat)

	sscanf(databuf+8,"%04x",&len);
	if (len != (strlen(databuf)-16))
	{
		loginf("bad EMSI_DAT length: %d/%d",len,strlen(databuf));
		SM_PROCEED(sendnak);
	}
	/* Some FD versions send length of the packet including the
	   trailing CR.  Arrrgh!  Dirty overwork follows: */
	if (*(p=databuf+strlen(databuf)-1) == '\r') *p='\0';
	sscanf(databuf+strlen(databuf)-4,"%04hx",&rcrc);
	*(databuf+strlen(databuf)-4)='\0';
	lcrc=crc16(databuf,strlen(databuf));
	if (lcrc != rcrc)
	{
		loginf("got EMSI_DAT packet \"%s\" with bad crc: %04x/%04x",
			printable(databuf,0),lcrc,rcrc);
		SM_PROCEED(sendnak);
	}
	if (scanemsidat(databuf+12) == 0)
	{
		SM_PROCEED(sendack);
	}
	else
	{
		logerr("could not parse EMSI_DAT packet \"%s\"",databuf);
		SM_ERROR;
	}

SM_STATE(sendnak)

	if (++tries > 9)
	{
		loginf("too many tries getting EMSI_DAT");
		SM_ERROR;
	}
	if (caller)
	{
		PUTSTR("**EMSI_NAKEEC3\r\021");
	}
	else
	{
		PUTSTR("**EMSI_REQA77E\r\021");
#ifdef SLAVE_SENDS_NAK_TOO
		if (tries > 1) PUTSTR("**EMSI_NAKEEC3\r\021");
#endif
	}
	SM_PROCEED(waitpkt);

SM_STATE(sendack)

	PUTSTR("**EMSI_ACKA490\r\021");
	PUTSTR("**EMSI_ACKA490\r\021");
	SM_SUCCESS;

SM_END

	free(databuf);

SM_RETURN


SM_DECL(txemsi,"txemsi")
SM_STATES
	senddata,waitpkt,waitchar,checkpkt,sendack
SM_NAMES
	"senddata","waitpkt","waitchar","checkpkt","sendack"
SM_EDECL

	int c;
	unsigned INT16 lcrc,rcrc;
	int standby=0,tries=0;
	char buf[13],*p;
	char trailer[8];

	p=buf;
	strncpy(buf,intro,sizeof(buf)-1);

SM_START(senddata)

SM_STATE(senddata)

	p=mkemsidat(caller);
	PUTCHAR('*');
	PUTCHAR('*');
	PUTSTR(p);
	sprintf(trailer,"%04X\r\021",crc16(p,strlen(p)));
	PUTSTR(trailer);
	free(p);
	SM_PROCEED(waitpkt);

SM_STATE(waitpkt)

	standby=0;
	SM_PROCEED(waitchar);

SM_STATE(waitchar)

	c=GETCHAR(8);
	if (c == TIMEOUT)
	{
		if (++tries > 9)
		{
			loginf("too many tries sending EMSI");
			SM_ERROR;
		}
		else {SM_PROCEED(senddata);}
	}
	else if (c < 0)
	{
		SM_ERROR;
	}
	else if ((c >= ' ') && (c <= '~'))
	{
		if (c == '*')
		{
			standby=1;
			p=buf;
			*p='\0';
		}
		else if (standby)
		{
			if ((p-buf) < (sizeof(buf)-1))
			{
				*p++=c;
				*p='\0';
			}
			if ((p-buf) >= (sizeof(buf)-1))
			{
				standby=0;
				SM_PROCEED(checkpkt);
			}
		}
	}
	else switch(c)
	{
	case DC1:	SM_PROCEED(waitchar);
			break;
	case '\n':
	case '\r':	standby=0;
			break;
	default:	standby=0;
			debug(10,"got '%s' from remote",
				printablec(c));
			break;
	}

	{SM_PROCEED(waitchar);}

SM_STATE(checkpkt)

	debug(10,"txemsi got: \"%s\"",buf);
	if (strncasecmp(buf,"EMSI_DAT",8) == 0)
	{
		SM_PROCEED(sendack);
	}
	else if (strncasecmp(buf,"EMSI_",5) == 0)
	{
		lcrc=crc16(buf,8);
		sscanf(buf+8,"%04hx",&rcrc);
		if (lcrc != rcrc)
		{
			loginf("got EMSI packet \"%s\" with bad crc: %04x/%04x",
				printable(buf,0),lcrc,rcrc);
			SM_PROCEED(senddata);
		}
		if (strncasecmp(buf,"EMSI_REQ",8) == 0)
		{
			SM_PROCEED(waitpkt);
		}
		if (strncasecmp(buf,"EMSI_ACK",8) == 0)
		{
			SM_SUCCESS;
		}
		else
		{
			SM_PROCEED(senddata);
		}
	}
	else
	{
			SM_PROCEED(waitpkt);
	}

SM_STATE(sendack)

	PUTSTR("**EMSI_ACKA490\r\021");
	PUTSTR("**EMSI_ACKA490\r\021");
/*	SM_PROCEED(senddata); */
	SM_PROCEED(waitpkt);

SM_END
SM_RETURN

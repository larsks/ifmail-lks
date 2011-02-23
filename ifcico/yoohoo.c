#include <stdlib.h>
#include <string.h>
#include "statetbl.h"
#include "xutil.h"
#include "lutil.h"
#include "ttyio.h"
#include "session.h"
#include "ftscprod.h"
#include "config.h"
#include "ftn.h"
#include "emsi.h"
#include "nodelist.h"
#include "version.h"

/*------------------------------------------------------------------------*/
/* YOOHOO<tm> CAPABILITY VALUES                                           */
/*------------------------------------------------------------------------*/
#define Y_DIETIFNA 0x0001  /* Can do fast "FTS-0001"  0000 0000 0000 0001 */
#define FTB_USER   0x0002  /* Full-Tilt Boogie        0000 0000 0000 0010 */
#define ZED_ZIPPER 0x0004  /* Does ZModem, 1K blocks  0000 0000 0000 0100 */
#define ZED_ZAPPER 0x0008  /* Can do ZModem variant   0000 0000 0000 1000 */
#define DOES_IANUS 0x0010  /* Can do Janus            0000 0000 0001 0000 */
#define DOES_HYDRA 0x0020  /* Can do Hydra            0000 0000 0010 0000 */
#define Bit_6      0x0040  /* reserved by FTSC        0000 0000 0100 0000 */
#define Bit_7      0x0080  /* reserved by FTSC        0000 0000 1000 0000 */
#define Bit_8      0x0100  /* reserved by FTSC        0000 0001 0000 0000 */
#define Bit_9      0x0200  /* reserved by FTSC        0000 0010 0000 0000 */
#define Bit_a      0x0400  /* reserved by FTSC        0000 0100 0000 0000 */
#define Bit_b      0x0800  /* reserved by FTSC        0000 1000 0000 0000 */
#define Bit_c      0x1000  /* reserved by FTSC        0001 0000 0000 0000 */
#define Bit_d      0x2000  /* reserved by FTSC        0010 0000 0000 0000 */
#define DO_DOMAIN  0x4000  /* Packet contains domain  0100 0000 0000 0000 */
#define WZ_FREQ    0x8000  /* WZ file req. ok         1000 0000 0000 0000 */

#define LOCALCAPS (Y_DIETIFNA|ZED_ZIPPER|ZED_ZAPPER|DO_DOMAIN)

extern unsigned INT16 crc16(char*,int);
extern int janus(void);
extern int rxwazoo(void);
extern int txwazoo(void);
extern int rxdietifna(void);
extern int txdietifna(void);
extern void rdoptions(node*);
extern int nodelock(faddr*);

static int rxyoohoo(void);
static int txyoohoo(void);
static void fillhello(unsigned short,char*);
static void checkhello(void);

static int iscaller;
static struct _hello {
	unsigned char data[128];
	unsigned char crc[2];
} hello;

typedef struct   _Hello {
    unsigned short signal;         /* always 'o'     (0x6f)                  */
    unsigned short hello_version;  /* currently 1    (0x01)                  */
    unsigned short product;        /* product code                           */
    unsigned short product_maj;    /* major revision of the product          */
    unsigned short product_min;    /* minor revision of the product          */
    unsigned char  my_name[60];    /* Other end's name, will include domain  */
                                   /* if DO_DOMAIN is set in capabilities    */
    unsigned char  sysop[20];      /* sysop's name                           */
    unsigned short my_zone;        /* 0== not supported                      */
    unsigned short my_net;         /* out primary net number                 */
    unsigned short my_node;        /* our primary node number                */
    unsigned short my_point;       /* 0 == not supported                     */
    unsigned char  my_password[8]; /* This isn't necessarily null-terminated */
    unsigned char  reserved2[8];   /* reserved by Opus                       */
    unsigned short capabilities;   /* see below                              */
    unsigned char  reserved3[12];  /* for non-Opus systems with "approval"   */
                                   /*         total size 128 bytes           */
} Hello;

Hello hello2;
Hello gethello2(unsigned char[]);

int rx_yoohoo(void)
{
	int rc;
	unsigned short capabilities,localcaps;
	char *pwd;
	fa_list *tmp;
	node *nlent;

	loginf("start inbound WaZOO session");

	pwd=NULL;
	localcaps=LOCALCAPS;
	if (localoptions & NOZMODEM) localcaps &= ~(ZED_ZAPPER|ZED_ZIPPER);
	if (localoptions & NOZEDZAP) localcaps &= ~ZED_ZAPPER;
	if (localoptions & NOJANUS) localcaps &= ~DOES_IANUS;
	if (localoptions & NOHYDRA) localcaps &= ~DOES_HYDRA;
	emsi_local_opts=0;
	emsi_remote_opts=0;
	iscaller=0;
	if ((rc=rxyoohoo()) == 0)
	{
		checkhello();
		capabilities=hello2.capabilities;
		if (capabilities & WZ_FREQ) session_flags |= SESSION_WAZOO;
		else session_flags &= ~SESSION_WAZOO;
		localcaps&=capabilities;
		if (localcaps & DOES_HYDRA) localcaps &= DOES_HYDRA;
		else if (localcaps & DOES_IANUS) localcaps &= DOES_IANUS;
		else if (localcaps & ZED_ZAPPER) localcaps &= ZED_ZAPPER;
		else if (localcaps & ZED_ZIPPER) localcaps &= ZED_ZIPPER;
		else if (localcaps & FTB_USER)   localcaps &= FTB_USER;
		else if (localcaps & Y_DIETIFNA) localcaps &= Y_DIETIFNA;
		if ((localoptions & NOFREQS) == 0) localcaps |= WZ_FREQ;
		else emsi_local_opts |= NRQ;

		if (((nlent=getnlent(remote->addr))) &&
		    (nlent->pflag != NL_DUMMY))
		{
			loginf("remote is a listed system");
			inbound=listinbound;
		}
		if (nlent) rdoptions(nlent);

		for (tmp=pwlist;tmp;tmp=tmp->next)
		if (metric(remote->addr,tmp->addr) == 0)
		{
			if (strncasecmp((char*)hello2.my_password,tmp->addr->name,8) != 0)
			{
				pwd="BAD_PASS";
				loginf("remote gave password \"%s\", expected \"%s\"",
					(char*)hello2.my_password,S(tmp->addr->name));
				localcaps=0;
			}
			else /* password did match, return it back */
			{
				loginf("remote gave correct password, protected WaZOO session");
				inbound=protinbound;
				pwd=tmp->addr->name;
				break;
			}
		}

		fillhello(localcaps,pwd);

		rc=txyoohoo();
	}
	if ((rc == 0) && ((localcaps & LOCALCAPS) == 0))
	{
		loginf("No common WaZOO protocols or bad password");
		return 0;
	}
	if (rc) return rc;
	session_flags |= SESSION_WAZOO;
	if (localcaps & DOES_IANUS) return janus();
	else if ((localcaps & ZED_ZAPPER) || (localcaps & ZED_ZIPPER))
	{
		if (localcaps & ZED_ZAPPER) emsi_local_protos=ZAP;
		else emsi_local_protos=ZMO;
		return rxwazoo();
	}
	else if (localcaps & Y_DIETIFNA) return rxdietifna();
	else logerr("WaZOO internal error - no proto for 0x%04xh",localcaps);
	return 1;
}

int tx_yoohoo(void)
{
	int rc;
	unsigned short capabilities;
	char *pwd;
	fa_list *tmp;

	loginf("start outbound WaZOO session");

	pwd=NULL;
	for (tmp=pwlist;tmp;tmp=tmp->next)
	if (metric(remote->addr,tmp->addr) == 0)
	{
		pwd=tmp->addr->name;
		break;
	}
	capabilities=LOCALCAPS;
	if (localoptions & NOZMODEM) capabilities &= ~(ZED_ZAPPER|ZED_ZIPPER);
	if (localoptions & NOZEDZAP) capabilities &= ~ZED_ZAPPER;
	if (localoptions & NOJANUS) capabilities &= ~DOES_IANUS;
	if (localoptions & NOHYDRA) capabilities &= ~DOES_HYDRA;
	if ((localoptions & NOFREQS) == 0) capabilities |= WZ_FREQ;
	else emsi_local_opts |= NRQ;
	fillhello(capabilities,pwd);
	iscaller=1;
	if ((rc=txyoohoo()) == 0)
	{
		rc=rxyoohoo();
		checkhello();
		capabilities=hello2.capabilities;
		if (capabilities & WZ_FREQ) session_flags |= SESSION_WAZOO;
		else session_flags &= ~SESSION_WAZOO;
	}
	if ((rc == 0) && ((capabilities & LOCALCAPS) == 0))
	{
		loginf("No common WaZOO protocols");
		return 0;
	}
	if (rc) return rc;
	session_flags |= SESSION_WAZOO;
	if (capabilities & DOES_IANUS) return janus();
	else if ((capabilities & ZED_ZAPPER) || (capabilities & ZED_ZIPPER))
	{
		if (capabilities & ZED_ZAPPER) emsi_local_protos=ZAP;
		else emsi_local_protos=ZMO;
		return txwazoo();
	}
	else if (capabilities & Y_DIETIFNA) return txdietifna();
	else logerr("WaZOO internal error - no proto for 0x%04xh",capabilities);
	return 1;
}

SM_DECL(rxyoohoo,"rxyoohoo")
SM_STATES
	sendenq,waitchar,getpacket,sendnak,sendack
SM_NAMES
	"sendenq","waitchar","getpacket","sendnak","sendack"
SM_EDECL

	int c;
	int count=0;
	unsigned short lcrc,rcrc;

SM_START(sendenq)

SM_STATE(sendenq)

	if (count++ > 9)
	{
		loginf("too many tries to get hello packet");
		SM_ERROR;
	}
	PUTCHAR(ENQ);
	SM_PROCEED(waitchar)

SM_STATE(waitchar)

	c=GETCHAR(10);
	if (c == TIMEOUT) {SM_PROCEED(sendenq);}
	else if (c < 0)
	{
		SM_ERROR;
	}
	else switch (c)
	{
	case 0x1f:	SM_PROCEED(getpacket); break;
	case YOOHOO:	SM_PROCEED(sendenq); break;
	default:	debug(10,"got '%s' waiting hello",
				printablec(c));
			SM_PROCEED(waitchar);
			break;
	}

SM_STATE(getpacket)

	GET((char*)&hello,sizeof(hello),30);
	if (STATUS)
	{
		SM_ERROR;
	}
	lcrc=crc16((char*)hello.data,sizeof(hello.data));
	rcrc=(hello.crc[0]<<8)+hello.crc[1];
	if (lcrc != rcrc)
	{
		debug(10,"crc does not match in hello packet: %04xh/%04xh",
			rcrc,lcrc);
		SM_PROCEED(sendnak);
	}
	else {SM_PROCEED(sendack);}

SM_STATE(sendnak)

	if (count++ > 9)
	{
		loginf("too many tries to get hello packet");
		SM_ERROR;
	}
	PUTCHAR('?');
	SM_PROCEED(waitchar);

SM_STATE(sendack)

	PUTCHAR(ACK);
	SM_SUCCESS;

SM_END
SM_RETURN


SM_DECL(txyoohoo,"txyoohoo")
SM_STATES
	sendyoohoo,waitenq,sendpkt,waitchar
SM_NAMES
	"sendyoohoo","waitenq","sendpkt","waitchar"
SM_EDECL

	int c;
	int count=0;
	int startstate;
	unsigned short lcrc;

	if (iscaller) startstate=sendpkt;
	else startstate=sendyoohoo;
	lcrc=crc16((char*)hello.data,sizeof(hello.data));
	hello.crc[0]=lcrc>>8;
	hello.crc[1]=lcrc&0xff;

SM_START(startstate)

SM_STATE(sendyoohoo)

	PUTCHAR(YOOHOO);
	SM_PROCEED(waitenq);

SM_STATE(waitenq)

	c=GETCHAR(10);
	if (c == TIMEOUT)
	{
		if (count++ > 9)
		{
			loginf("timeout waiting ENQ");
			SM_ERROR;
		}
		else
		{
			SM_PROCEED(sendyoohoo);
		}
	}
	else if (c < 0)
	{
		SM_ERROR;
	}
	else switch (c)
	{
	case ENQ:	SM_PROCEED(sendpkt);
	case YOOHOO:	SM_PROCEED(waitenq);
	default:	debug(10,"got '%s' waiting hello ACK",
				printablec(c));
			SM_PROCEED(waitchar);
			break;
	}

SM_STATE(sendpkt)

	if (count++ > 9)
	{
		loginf("too many tries to send hello packet");
		SM_ERROR;
	}

	PUTCHAR(0x1f);
	PUT((char*)&hello,sizeof(hello));
	if (STATUS) {SM_ERROR;}
	SM_PROCEED(waitchar);

SM_STATE(waitchar)

	c=GETCHAR(30);
	if (c == TIMEOUT)
	{
		loginf("timeout waiting hello ACK");
		SM_ERROR;
	}
	else if (c < 0)
	{
		SM_ERROR;
	}
	else switch (c)
	{
	case ACK:	SM_SUCCESS; break;
	case '?':	SM_PROCEED(sendpkt); break;
	case ENQ:	SM_PROCEED(sendpkt); break;
	default:	debug(10,"got '%s' waiting hello ACK",
				printablec(c));
			SM_PROCEED(waitchar);
			break;
	}

SM_END
SM_RETURN


void fillhello(capabilities,password)
unsigned short capabilities;
char *password;
{
	faddr *best;
	unsigned short majver,minver;

	debug(10,"fillhello(0x%04hx)",capabilities);

	best=bestaka_s(remote->addr);

	sscanf(version,"%hd.%hd",&majver,&minver);
	memset(&hello,0,sizeof(hello));
	hello.data[0]='o'; /* signal */
	hello.data[2]=1; /* hello-version */
	hello.data[4]=PRODCODE; /* product */
	hello.data[6]=majver&0xff; /* prod-ver-major */
	hello.data[7]=majver>>8; /* prod-ver-major */
	hello.data[8]=minver&0xff; /* prod-ver-minor */
	hello.data[9]=minver>>8; /* prod-ver-minor */
	strncpy((char*)hello.data+10,name?name:"Unavailable",59); /* name */
	if (name)
	{
		hello.data[10+strlen(name)]='\0';
		strncpy((char*)hello.data+11+strlen(name),
			best->domain,58-strlen(name)); /* domain */
	} 
	else strncpy((char*)hello.data+22,best->domain,47); /* domain */
	strncpy((char*)hello.data+70,sysop?sysop:"Unavailable",19); /* sysop */
	hello.data[90]=best->zone&0xff; /* zone */
	hello.data[91]=best->zone>>8; /* zone */
	hello.data[92]=best->net&0xff; /* net */
	hello.data[93]=best->net>>8; /* net */
	hello.data[94]=best->node&0xff; /* node */
	hello.data[95]=best->node>>8; /* node */
	hello.data[96]=best->point&0xff; /* point */
	hello.data[97]=best->point>>8; /* point */

	if (password) strncpy((char*)hello.data+98,password,8);

	hello.data[114]=capabilities&0xff; /* capabilities */
	hello.data[115]=capabilities>>8; /* capabilities */
	debug(10,"filled hello \"%s\"",printable((char*)hello.data,128));
}

void checkhello(void)
{
	unsigned short i,majver,minver;
	fa_list **tmpl,*tmpn;
	faddr remaddr;
	char *prodnm, *q;

	debug(10,"check hello \"%s\"",printable((char*)hello.data,128));

	hello2=gethello2(hello.data);

	if ((hello2.signal != 0x6f) ||
		(hello2.hello_version != 0x01))
	{
		loginf("got \"%s\" instead of \"o\\000\\001\000\"",
			printable((char*)hello.data,3));
	}
	for (i=0;ftscprod[i].name;i++)
		if (ftscprod[i].code == hello2.product) break;
	prodnm=ftscprod[i].name;
	majver=hello2.product_maj;
	minver=hello2.product_min;
	remaddr.zone=hello2.my_zone;
	remaddr.net=hello2.my_net;
	remaddr.node=hello2.my_node;
	remaddr.point=hello2.my_point;
	remaddr.name=NULL;
	remaddr.domain=NULL;
	if (hello2.my_name[0])
		remaddr.domain=hello2.my_name+(strlen(hello2.my_name))+1;
	if (remaddr.domain[0]) {
		if ((q=strchr(remaddr.domain,'.')))
			*q='\0';
	} else {
		remaddr.domain=NULL;
	}
	if (remote)
		debug(10,"remote known     address: %s",ascfnode(remote->addr,0x1f));
	debug(10,"remote presented address: %s",ascfnode(&remaddr,0x1f));
	for (tmpl=&remote;*tmpl;tmpl=&((*tmpl)->next));
	if ((remote == NULL) || (metric(remote->addr,&remaddr) != 0))
	{
		(*tmpl)=(fa_list*)xmalloc(sizeof(fa_list));
		(*tmpl)->next=NULL;
		(*tmpl)->addr=(faddr*)xmalloc(sizeof(faddr));
		(*tmpl)->addr->zone=remaddr.zone;
		(*tmpl)->addr->net=remaddr.net;
		(*tmpl)->addr->node=remaddr.node;
		(*tmpl)->addr->point=remaddr.point;
		(*tmpl)->addr->domain=xstrcpy(remaddr.domain);
	}
	else
	{
		tmpl=&remote;
		debug(10,"using single remote address");
	}

	for (tmpn=remote;tmpn;tmpn=tmpn->next)
		(void)nodelock(tmpn->addr);
		/* lock all remotes, ignore locking result */

	loginf("remote  address: %s",ascfnode(remote->addr,0x1f));
	loginf("remote password: %s",(char*)hello2.my_password);
	if (hello2.product < 0x0100)
		loginf("remote     uses: %s [%02X] version %d.%d",
			prodnm?prodnm:"<unknown program>",hello2.product,
			majver,minver);
	else loginf("remote     uses: %s [%04X] version %d.%d",
			prodnm?prodnm:"<unknown program>",hello2.product,
			majver,minver);
	loginf("remote   system: %s",(char*)hello2.my_name);
	loginf("remote operator: %s",(char*)hello2.sysop);
}

Hello gethello2(Hellop)
unsigned char Hellop[];
{
        int	i;
        Hello	p;

        p.signal=Hellop[0]+(Hellop[1]<<8);
        p.hello_version=Hellop[2]+(Hellop[3]<<8);
        p.product=Hellop[4]+(Hellop[5]<<8);
        p.product_maj=Hellop[6]+(Hellop[7]<<8);
        p.product_min=Hellop[8]+(Hellop[9]<<8);
        for (i=0;i<60;i++)
           p.my_name[i]=Hellop[10+i];
        for (i=0;i<20;i++)
           p.sysop[i]=Hellop[70+i];
        p.my_zone=Hellop[90]+(Hellop[91]<<8);
        p.my_net=Hellop[92]+(Hellop[93]<<8);
        p.my_node=Hellop[94]+(Hellop[95]<<8);
        p.my_point=Hellop[96]+(Hellop[97]<<8);
        for (i=0;i<8;i++)
           p.my_password[i]=Hellop[98+i];
        for (i=0;i<8;i++)
           p.reserved2[i]=Hellop[106+i];
        p.capabilities=Hellop[114]+(Hellop[115]<<8);
        for (i=0;i<12;i++)
           p.reserved3[i]=Hellop[116+i];

        return p;
}

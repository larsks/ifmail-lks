/*
        contributed by Stanislav Voronyi <stas@uanet.kharkov.ua>
*/

#ifdef HAS_TCP

#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <utime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#if defined(HAS_STATFS)
#if defined(STATFS_IN_VFS_H)
#include <sys/vfs.h>
#elif defined(STATFS_IN_STATFS_H)
#include <sys/statfs.h>
#elif defined(STATFS_IN_STATVFS_H)
#include <sys/statvfs.h>
#elif defined(STATFS_IN_MOUNT_H)
#include <sys/param.h>
#include <sys/mount.h>
#else
#error No include for statfs() call defined
#endif
#elif defined(HAS_STATVFS)
#include <sys/statvfs.h>
#endif
#include "xutil.h"
#include "lutil.h"
#include "ttyio.h"
#include "session.h"
#include "config.h"
#include "emsi.h"

#define TCP_CMD	0x87
#define TCP_DATA 0xe1

static FILE *fout;
static FILE *in;
static char txbuf[2048];
static char rxbuf[2048];
static int rx_type;
static long startime,endtime,rxbytes,sbytes;

static int sendfile(char *,char *);
static int finsend(void);
static int receivefile(char *,time_t,off_t);
static int resync(off_t);
static int closeit(int);
static int tcp_sblk(char *,int,int);
static int tcp_rblk(char *,int *);
static int getsync();

extern FILE *openfile(char*,time_t,off_t,off_t*,int(*)(off_t));
extern int closefile(int);

int tcpsndfiles(file_list*);
int tcpsndfiles(lst)
file_list *lst;
{
	int rc=0,maxrc=0;
	file_list *tmpf;

	loginf("start TCP send%s",lst?"":" (dummy)");

	if(getsync()){
		logerr("cant get synchronization");
		return 1;
	}
	for (tmpf=lst;tmpf && (maxrc == 0);tmpf=tmpf->next)
	{
		if (tmpf->remote)
		{
			rc=sendfile(tmpf->local,tmpf->remote);
			rc=abs(rc);
			if (rc > maxrc) maxrc=rc;
			if (rc == 0) execute_disposition(tmpf);
		}
		else if (maxrc == 0) execute_disposition(tmpf);
	}
	if (maxrc < 2)
	{
		rc=finsend();
		rc=abs(rc);
	}
	if (rc > maxrc) maxrc=rc;

	loginf("TCP send rc=%d",maxrc);
	return maxrc;
}

int tcprcvfiles(void);
int tcprcvfiles(void)
{
	int rc,bufl;
	long filesize,filetime;
	char *filename,*p;	

	loginf("start TCP receive");
	if(getsync()){
		logerr("cant get synchronization");
		return 1;
	}
next:
	if((rc=tcp_rblk(rxbuf,&bufl))==0){
		if(strncmp(rxbuf,"SN",2)==0){
			rc=tcp_sblk("RN",2,TCP_CMD);
			return rc;
		}
		else if(*rxbuf=='S'){
			p=strchr(rxbuf+2,' ');
			*p=0;
			filename=xstrcpy(rxbuf+2);
			p++;
			filesize=strtol(p,&p,10);
			filetime=strtol(++p,(char **)NULL,10);			
		}
		else return rc==0?1:rc;

	if(strlen(filename) && filesize && filetime)
		rc=receivefile(filename,filetime,filesize);
	if (fout)
	{ 
		if (closeit(0)) {
			logerr("Error closing file");
		}
		(void)tcp_sblk("FERROR",6,TCP_CMD);
	}
	else
		goto next;
	}

	loginf("TCP receive rc=%d",rc);
	return abs(rc);
}

static int sendfile(ln,rn)
char *ln,*rn;
{
	int rc=0;
	struct stat st;
	struct flock fl;
	int bufl;
	long offset;
	int sverr;

	fl.l_type=F_RDLCK;
	fl.l_whence=0;
	fl.l_start=0L;
	fl.l_len=0L;
	if ((in=fopen(ln,"r")) == NULL)
	{
		sverr=errno;
		logerr("$tcpsend cannot open file %s, skipping",ln);
		if (sverr == ENOENT) return 0;
		else return 1;
	}
	if (fcntl(fileno(in),F_SETLK,&fl) != 0)
	{
		loginf("$tcpsend cannot lock file %s, skipping",ln);
		fclose(in);
		return 1;
	}
	if (stat(ln,&st) != 0)
	{
		loginf("$cannot access \"%s\", skipping",ln);
		fclose(in);
		return 1;
	}
	
	if(st.st_size>0){
		loginf("TCP send \"%s\" as \"%s\" (%lu bytes)",
			ln,rn,(unsigned long)st.st_size);
		(void)time(&startime);
	}
	else{
		loginf("File \"%s\" has 0 size, skiped",ln);
		return 0;
	}

	sprintf(txbuf,"S %s %lu %lu",rn,(unsigned long)st.st_size,st.st_mtime);
	bufl=strlen(txbuf);
	rc=tcp_sblk(txbuf,bufl,TCP_CMD);
	rc=tcp_rblk(rxbuf,&bufl);
	if (strncmp(rxbuf,"RS",2)==0)
	{
		loginf("file %s considered normally sent",ln);
		return 0;
	}
	else if (strncmp(rxbuf,"RN",2)==0)
	{
		loginf("remote refusing receiving, aborting",ln);
		return 2;
	}
	else if(strncmp(rxbuf,"ROK",3)==0){
		if(bufl > 3 && rxbuf[3]==' '){
			offset=strtol(rxbuf+4,(char **)NULL,10);
			if(fseek(in,offset,SEEK_SET)!=0){
				logerr("$tcpsend cannot seek in file %s",ln);
				return 1;
			}
		}
		else offset=0;
	}
	else return rc;

	while((bufl=fread(&txbuf,1,1024,in))!=0){
		if((rc=tcp_sblk(txbuf,bufl,TCP_DATA))>0)
			break;
	}
	fclose(in);
	if(rc==0){
		strcpy(txbuf,"EOF");
		rc=tcp_sblk(txbuf,3,TCP_CMD);
		rc=tcp_rblk(rxbuf,&bufl);
	}

	if (rc == 0 && strncmp(rxbuf,"FOK",3)==0)
	{
		(void)time(&endtime);
		if ((startime=endtime-startime) == 0) startime=1;
		debug(7,"st_size %d, offset %d",st.st_size,offset);
		loginf("sent %lu bytes in %ld seconds (%ld cps)",
			(unsigned long)st.st_size-offset,
			startime,
			(long)(st.st_size-offset)/startime);
		return 0;
	}
	else if(strncmp(rxbuf,"FERROR",6)==0){
		logerr("$tcpsend remote file error",ln);
		return rc==0?1:rc;
	}
	else return rc==0?1:rc;
}

static int resync(off)
off_t off;
{
	sprintf(txbuf,"ROK %ld",(long)off);
	return 0;
}

static int closeit(success)
int success;
{
	int rc;

	rc=closefile(success);
	fout=NULL;
	sbytes=rxbytes-sbytes;
	(void)time(&endtime);
	if ((startime=endtime-startime) == 0L) startime=1L;
	loginf("%s %lu bytes in %ld seconds (%ld cps)",
		success?"received":"dropped after",
		sbytes,startime,sbytes/startime);
	return rc;
}

static int finsend()
{
int rc,bufl;
rc=tcp_sblk("SN",2,TCP_CMD);
if(rc) return rc;
rc=tcp_rblk(rxbuf,&bufl);
if(strncmp(rxbuf,"RN",2)==0)
	return rc;
else
	return 1;
}

static int receivefile(fn,ft,fs)
char *fn;
time_t ft;
off_t fs;
{
	int rc,bufl;

	loginf("TCP receive \"%s\" (%lu bytes)",fn,fs);
	strcpy(txbuf,"ROK");
	fout=openfile(fn,ft,fs,&rxbytes,resync);
	if ( !fout) return 1;
	(void)time(&startime);
	sbytes=rxbytes;

	if (fs == rxbytes) {
		loginf("Skipping %s", fn);
		closeit(1);
		rc=tcp_sblk("RS",2,TCP_CMD);
		return rc;
	}
	bufl=strlen(txbuf);
	rc=tcp_sblk(txbuf,bufl,TCP_CMD);

	while((rc=tcp_rblk(rxbuf,&bufl))==0){
		if(rx_type==TCP_CMD)
			break;
		if(fwrite(rxbuf,1,bufl,fout)!=bufl)
			break;
		rxbytes+=bufl;
	}
	if(rc) return rc;
	if(rx_type==TCP_CMD && bufl==3 && strncmp(rxbuf,"EOF",3)==0){
		if(ftell(fout)==fs){
			closeit(1);
			rc=tcp_sblk("FOK",3,TCP_CMD);
			return rc;
		}
		else	return 1;
	}
	else	return 1;		
}

static int
tcp_sblk(buf,len,typ)
char *buf;
int len,typ;
{
PUTCHAR(0xc6);
PUTCHAR(typ);
PUTCHAR((len>>8)&0x0ff);
PUTCHAR(len&0x0ff);
PUT(buf,len);
PUTCHAR(0x6c);
return tty_status;
}

static int
tcp_rblk(buf,len)
char *buf;
int *len;
{
int c;

TTYWAIT(1);

*len=0;
c=GETCHAR(120);
if(c!=0xc6)
	return c;
c=GETCHAR(120);
rx_type=c;
if(c!=TCP_CMD && c!=TCP_DATA)
	return c;
c=GETCHAR(120);
*len=c<<8;
c=GETCHAR(120);
*len+=c;
GET(buf,*len,120);
c=GETCHAR(120);
if(c!=0x6c)
	return c;
return tty_status;
}

static int
getsync()
{
int c;
PUTCHAR(0xaa);
PUTCHAR(0x55);
TTYWAIT(1);
debug(9,"getsync try to synchronize");

gs:
if(tty_status==STAT_TIMEOUT){
debug(9,"getsync failed");
	return 1;}
while((c=GETCHAR(120))!=0xaa)
if(tty_status==STAT_TIMEOUT){
debug(9,"getsync failed");
	return 1;}
if((c=GETCHAR(120))!=0x55)
	goto gs;
debug(9,"getsync ok");
return tty_status;
}
#endif /* HAS_TCP */

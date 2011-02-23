#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "directory.h"
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef HAS_SYSLOG
#include <syslog.h>
#endif
#include "mygetopt.h"
#include "lutil.h"
#include "xutil.h"
#include "ftn.h"
#include "config.h"
#include "scanout.h"
#include "version.h"
#include "trap.h"

#define CURRENT "current.tmp"
#define MAXPACK 12
#define PACKFACTOR 0.4

extern int f_lock(char *);
extern void funlock(int);
extern int nodelock(faddr *);
extern int nodeulock(faddr *);
extern int execute(char *,char *,char *,char *,char *,char *);
extern unsigned INT32 sequencer(void);
extern char *arcname(faddr *,char);
extern char *floname(faddr *,char);
extern char *splname(faddr *,char);
extern char *pkdname(faddr *,char);

extern char *logname;
extern int fakeoutbound;

static int each1(faddr*,char,int,char*);
static int each2(faddr*,char,int,char*);
static int packets=0,files=0,directions=0;
static int forced=0;
static char *dow[] = {"su","mo","tu","we","th","fr","sa"};
static char *ext;

void usage(name)
char *name;
{
	confusage("-N -f");
	fprintf(stderr,"-N              process /tmp/ifmail directory\n");
	fprintf(stderr,"-f              pack *.?ut files too\n");
}

int main(argc,argv)
int argc;
char *argv[];
{
	int c,rc;
	time_t tt;
	struct tm *ptm;

#if defined(HAS_SYSLOG) && defined(MAILLOG)
	logfacility=MAILLOG;
#endif

	setmyname(argv[0]);
	catch(myname);
	while ((c=getopt(argc,argv,"x:I:hNf")) != -1)
	if (confopt(c,optarg)) switch (c)
	{
		case 'N':	fakeoutbound=1; break;
		case 'f':	forced=1; break;
		default:	usage(); exit(1);
	}

	umask(066);

	if ((rc=readconfig()))
	{
		logerr("Error getting configuration, aborting\n");
		return rc;
	}

	(void)time(&tt);
	ptm=localtime(&tt);
	ext=dow[ptm->tm_wday];
	debug(3,"today's arcmail extention \"%s\"",ext);

	if ((rc=scanout(each1)))
	{
		logerr("Error scanning outbound (pass 1), aborting\n");
		return rc;
	}

	if ((rc=scanout(each2)))
	{
		logerr("Error scanning outbound (pass 2), aborting\n");
		return rc;
	}

	if (packets) loginf("packed %d packet%s into %d file%s for %d feed%s, rc=%d",
		packets,(packets==1)?"":"s",
		files,(files==1)?"":"s",
		directions,(directions==1)?"":"s",
		rc);
	return rc;
}

/* pass 1: move all pending packets into .?pk direcrories (with locking) */

static int each1(addr,flavor,isflo,pktfn)
faddr *addr;
char flavor;
int isflo;
char *pktfn;
{
	char buf[25],*p;
	int pl;
	struct stat stbuf;

	switch (isflo)
	{
	case OUT_FLO:
	case OUT_REQ:
		return 0;

	case OUT_ARC:
		if (strncasecmp(pktfn+strlen(pktfn)-3,ext,2) == 0) return 0;
		if ((stat(pktfn,&stbuf) == 0) && (stbuf.st_size == 0))
		{
			debug(3,"unlink non-todays empty arcmail \"%s\"",
				pktfn);
			unlink(pktfn);
		}
		return 0;

	case OUT_PKT:
		if (!forced) return 0;

		debug(3,"pass 1: packet \"%s\" to node %s",
			S(pktfn),ascfnode(addr,0x1f));

		if ((p=strrchr(pktfn,'/')))
		{
			*p='\0';
			if (chdir(pktfn))
			{
				logerr("$cannot chdir(\"%s\")",S(pktfn));
				return 1;
			}
			p++;
		}
		else
		{
			logerr("cannot be: packet name \"%s\" without slash",
				S(pktfn));
			return 1;
		}
		if (nodelock(addr))
		{
			debug(3,"system %s locked, skipping",
				ascfnode(addr,0x1f));
			return 0;
		}
		if ((pl=f_lock(p)) == -1)
		{
			debug(3,"cannot lock packet \"%s\", skipping",
				S(p));
			nodeulock(addr);
			return 0;
		}

		strcpy(buf,pkdname(addr,flavor));
		mkdir(buf,0777);
		sprintf(buf+strlen(buf),"/%08lx.pkt",
				(unsigned long)sequencer());

		if (rename(p,buf) < 0)
		{
			logerr("$cannot rename \"%s\" to \"%s\"",
				S(p),buf);
		}
		else
		{
			debug(3,"renamed \"%s\" to \"%s\"",
				S(p),buf);
		}
		funlock(pl);
		nodeulock(addr);
		return 0;

	case OUT_DIR:
		debug(3,"pass 1: directory \"%s\" to node %s",
			S(pktfn),ascfnode(addr,0x1f));

		if (chdir(pktfn))
		{
			logerr("$cannot chdir(\"%s\")",S(pktfn));
			return 1;
		}
		if (stat(CURRENT,&stbuf))
		{
			return 0;
		}
		if (nodelock(addr))
		{
			debug(3,"system %s locked, skipping",
				ascfnode(addr,0x1f));
			return 0;
		}
		if ((pl=f_lock(CURRENT)) == -1)
		{
			debug(3,"cannot lock \"%s\", ignore",S(pktfn));
			nodeulock(addr);
			return 0;
		}

		sprintf(buf,"%08lx.pkt",(unsigned long)sequencer());

		if (rename(CURRENT,buf) < 0)
		{
			logerr("$cannot rename \"%s\" to \"%s\"",
				CURRENT,buf);
		}
		else
		{
			debug(3,"renamed \"%s\" to \"%s\"",
				CURRENT,buf);
		}
		funlock(pl);
		nodeulock(addr);
		return 0;

	default:		/* this cannot be, abort */
		logerr("Cannot be: isflo=%d",isflo);
		return 1;
	}
}

/* pass 2: pack prepared packets (no locking needed) */

static int each2(addr,flavor,isflo,pktfn)
faddr *addr;
char flavor;
int isflo;
char *pktfn;
{
	int rc=0;
	int absent,needadd,numpkts;
	int didpack=0;
	char c,*p,*q,*pkt;
	char *flofn,*arcfn,*splfn,*pktlist=NULL;
	char buf[512];
	FILE *fp=NULL;
	DIR *dp=NULL;
	struct dirent *de;
	struct flock fl;
	struct stat stbuf;
	off_t arcsize,sumsize;

	if (isflo != OUT_DIR) return 0;

	if (chdir(pktfn))
	{
		logerr("$cannot chdir to \"%s\"",S(pktfn));
		return 1;
	}

	if (nodelock(addr))
	{
		debug(3,"system %s locked, skipping",
			ascfnode(addr,0x1f));
		return 0;
	}

	debug(3,"pass 2: %s \"%s\" to node %s",
		(isflo == OUT_PKT)?"packet":"directory",
		S(pktfn),ascfnode(addr,0x1f));

	arcfn=xstrcpy(arcname(addr,flavor));
	flofn=xstrcpy(floname(addr,flavor));
	splfn=xstrcpy(splname(addr,flavor));

	fl.l_type=F_WRLCK;
	fl.l_whence=0;
	fl.l_start=0L;
	fl.l_len=0L;

	if ((fp=fopen(flofn,"r+")))
	{
		if (fcntl(fileno(fp),F_SETLK,&fl) < 0)
		{
			if (errno != EAGAIN)
				logerr("$Unable to lock flo file \"%s\"",S(flofn));
			else
				debug(3,"skipping locked flo file \"%s\"",S(flofn));
			goto leave;
		}
		if (stat(flofn,&stbuf) != 0)
		{
			debug(3,"$Unable to access flo file \"%s\"",S(flofn));
			goto leave;
		}
	}
	p=arcfn+strlen(arcfn)-1;

	absent=1;
	if (fp) while (fgets(buf,sizeof(buf)-1,fp))
	{
		if ((buf[0] != '~') &&
		    (strncmp(buf+1,arcfn,strlen(arcfn)-1) == 0))
		{
			absent=0;
			c=buf[strlen(arcfn)];
			if (*p < c) *p=c;
			debug(3,"old arcmail file \"%s\"",
				S(arcfn));
		}
	}

	needadd=0;
	if (absent || (stat(splfn,&stbuf) == 0))
	{
		debug(3,"new arcmail file needed");
		needadd=1;
		unlink(splfn);
		while (stat(arcfn,&stbuf) == 0)
			if (*p == '9') *p='a';
			else if (*p < 'z') (*p)++;
			else break;
	}

	if ((dp=opendir(pktfn)) == NULL)
	{
		logerr("$cannot open directory \"%s\"",S(pktfn));
		rc=1;
		goto leave;
	}

	while (1)
	{
		if (stat(arcfn,&stbuf) || (*p == 'z')) arcsize=0L;
		else arcsize=stbuf.st_size;

		if ((maxfsize > 0) && (arcsize > maxfsize))
		{
			debug(3,"arc file \"%s\" too big, make new",
				arcfn);
			while (stat(arcfn,&stbuf) == 0)
				if (*p == '9') *p='a';
				else if (*p < 'z') (*p)++;
				else break;
			needadd=1;
			continue;
		}

		debug(3,"using .flo file \"%s\", arc file \"%s\" (%lu)%s",
			S(flofn),S(arcfn),(unsigned long)arcsize,
			needadd?", needadd":"");

		if (pktlist) free(pktlist);
		pktlist=NULL;
		sumsize=0L;
		numpkts=0;
		do
		{
			de=readdir(dp);
			if (de == NULL) break;
			if ((strspn(de->d_name,"0123456789abcdefABCDEF") != 8) ||
			    (strcasecmp(de->d_name+8,".pkt") != 0))
				continue;

			if (stat(de->d_name,&stbuf))
			{
				logerr("$cannot stat \"%s\"",de->d_name);
				rc=1;
				goto leave;
			}
			sumsize += stbuf.st_size;
			if (pktlist) pktlist=xstrcat(pktlist," ");
			pktlist=xstrcat(pktlist,de->d_name);
			numpkts++;
		}
		while ((numpkts < MAXPACK) &&
		       ((sumsize * PACKFACTOR) < (maxfsize-arcsize)));
		/* At least one packet is added */

		debug(3,"pktlist: \"%s\"",S(pktlist));

		if (pktlist == NULL) goto leave;

		rc=execute(packer,arcfn,pktlist,"/dev/null",logname,logname);

		if (rc == 0)
		{
			packets+=numpkts;
			files++;
			didpack=1;
			for (pkt=strtok(pktlist," ");pkt;pkt=strtok(NULL," "))
			{
				debug(3,"unlink \"%s\"",pkt);
				unlink(pkt);
			}
			if (needadd)
			{
				if (stat(arcfn,&stbuf) == 0)
					debug(3,"adding \"%s\" (%lu) to flo",
						S(arcfn),
						(unsigned long)stbuf.st_size);
				if (fp == NULL)
				{
					fp=fopen(flofn,"w");
					if (fp == NULL)
					{
						logerr("$could not open flo file \"%s\"",
							S(flofn));
						rc=1;
						goto leave;
					}
					if (fcntl(fileno(fp),F_SETLK,&fl) < 0)
					{
						if (errno != EAGAIN)
							logerr("$Unable to lock flo file \"%s\"",S(flofn));
						else
							debug(3,"skipping locked flo file \"%s\"",S(flofn));
						goto leave;
					}
				}
				if (dosoutbound)
				{
					fprintf(fp,"#%s",dosoutbound);
					if (*(dosoutbound+
						strlen(dosoutbound)-1) != '\\')
						fputc('\\',fp);
					if (*(q=arcfn+strlen(outbound)) == '/')
						q++;
					for (;*q;q++)
						fputc((*q == '/')?'\\':*q,fp);
					fputc('\r',fp);
					fputc('\n',fp);
				}
				else fprintf(fp,"#%s\n",arcfn);
				needadd=0;
			}
		}
		else goto leave;
	}

leave:
	directions+=didpack;
	debug(3,"leaving each2 with rc=%d",rc);
	nodeulock(addr);
	if (pktlist) free(pktlist);
	if (fp) fclose(fp);
	if (dp) closedir(dp);
	free(arcfn);
	free(flofn);
	free(splfn);

	return rc;
}

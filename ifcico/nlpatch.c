#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef HAS_SYSLOG
#include <syslog.h>
#endif
#include "mygetopt.h"
#include "xutil.h"
#include "lutil.h"
#include "version.h"
#include "config.h"

#ifndef BLKSIZ
#define BLKSIZ 512
#endif

extern unsigned INT16 crc16tab[];
#define updcrc(cp, crc) ( crc16tab[((crc >> 8) & 255) ^ cp] ^ (crc << 8))

void usage(void)
{
	confusage("<nodelist> <nodediff>");
}

static int apply(char*,char*,char*);

int main(argc,argv)
int argc;
char *argv[];
{
	int c,rc;
	char *nl,*nd,*nn;
	char *p,*q;

#if defined(HAS_SYSLOG) && defined(CICOLOG)
	logfacility=CICOLOG;
#endif

	setmyname(argv[0]);
	while ((c=getopt(argc,argv,"x:I:h")) != -1)
	if (confopt(c,optarg)) switch (c)
	{
		default:	usage(); exit(1);
	}

	if (((nl=argv[optind++]) == NULL) ||
	    ((nd=argv[optind++]) == NULL) ||
	    (argv[optind]))
	{
		usage(); exit(1);
	}

	if ((rc=readconfig()))
	{
		fprintf(stderr,"Error getting configuration, aborting\n");
		return rc;
	}

	if (((p=strrchr(nl,'.'))) && ((q=strrchr(nd,'.'))) &&
	    (strlen(p) == strlen(q)))
	{
		nn=xstrcpy(nl);
		p=strrchr(nn,'.')+1;
		q++;
		strcpy(p,q);
	}
	else nn=xstrcpy("newnodelist");
	if (strcmp(nl,nn) == 0)
	{
		loginf("attempt to update notelist to the same version");
		exit(2);
	}
	loginf("patching %s with %s to %s",nl,nd,nn);
	rc=apply(nl,nd,nn);
	if (rc) unlink(nn);
	free(nn);

	return rc;
}

int apply(nl,nd,nn)
char *nl,*nd,*nn;
{
	FILE *fo,*fd,*fn;
	unsigned char cmdbuf[BLKSIZ];
	unsigned char lnbuf[BLKSIZ];
	int i,count;
	int ac=0,cc=0,dc=0;
	int rc=0;
	int firstline=1;
	unsigned INT16 theircrc=0,mycrc=0;
	unsigned char *p;

	if (((fo=fopen(nl,"r")) == NULL) ||
	    ((fd=fopen(nd,"r")) == NULL) ||
	    ((fn=fopen(nn,"w")) == NULL))
	{
		logerr("$cannot open some file(s)");
		return 2;
	}

	if ((fgets(cmdbuf,sizeof(cmdbuf)-1,fd) == NULL) ||
	    (fgets(lnbuf,sizeof(cmdbuf)-1,fo) == NULL) ||
	    (strcmp(cmdbuf,lnbuf) != 0))
	{
		rc=6;
	}
	else
	{
		rewind(fo);
		rewind(fd);
		while ((rc == 0) && fgets(cmdbuf,sizeof(cmdbuf)-1,fd))
		switch (cmdbuf[0])
		{
		case ';':
			debug(20,"COM: %s",cmdbuf);
			break;
		case 'A':
			count=atoi(cmdbuf+1);
			ac+=count;
			debug(20,"ADD %d: %s",count,cmdbuf);
			for (i=0;(i < count) && (rc == 0);i++)
			if (fgets(lnbuf,sizeof(lnbuf)-1,fd))
			{
				if (firstline)
				{
					firstline=0;
					if ((p=strrchr(lnbuf,':')))
					{
						theircrc=atoi(p+1);
					}
				}
				else
				{
					for (p=lnbuf;*p;p++)
						mycrc=updcrc(*p,mycrc);
				}
				fputs(lnbuf,fn);
			}
			else rc=3;
			break;
		case 'D':
			count=atoi(cmdbuf+1);
			dc+=count;
			debug(20,"DEL %d: %s",count,cmdbuf);
			for (i=0;(i < count) && (rc == 0);i++)
			if (fgets(lnbuf,sizeof(lnbuf)-1,fo) == NULL) rc=3;
			break;
		case 'C':
			count=atoi(cmdbuf+1);
			cc+=count;
			debug(20,"CPY %d: %s",count,cmdbuf);
			for (i=0;(i < count) && (rc == 0);i++)
			if (fgets(lnbuf,sizeof(lnbuf)-1,fo))
			{
				for (p=lnbuf;*p;p++)
					mycrc=updcrc(*p,mycrc);
				fputs(lnbuf,fn);
			}
			else rc=3;
			break;
		default:
			rc=5;
			break;
		}
	}

	fclose(fo);
	fclose(fd);
	fclose(fn);

	debug(20,"theircrc=%hu, mycrc=%hu",theircrc,mycrc);

	if ((rc == 0) && (mycrc != theircrc)) rc=4;

	if (rc == 3)
		loginf("could not read some of the files");
	else if (rc == 4)
		loginf("crc of resulting file %hu does not match official %hu",
			mycrc,theircrc);
	else if (rc == 5)
		loginf("unknown command line: \"%s\"",cmdbuf);
	else if (rc == 6)
		loginf("diff does not match old list");
	else
		loginf("copied %d, added %d, deleted %d, difference %d, rc=%d",
			cc,ac,dc,ac-dc,rc);
	return rc;
}

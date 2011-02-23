#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <utime.h>
#include <fcntl.h>
#include "xutil.h"
#include "lutil.h"
#include "config.h"

extern unsigned INT32 sequencer(void);

static FILE *infp=NULL;
static char *infpath=NULL;
static time_t intime;
static int isfreq;
char *freqname=NULL;

/*
   Try to find present (probably incomplete) file with the same timestamp
   (it might be renamed), open it for append and store resync offset.
   Store 0 in resync offset if the file is new. Return FILE* or NULL.
   resync() must accept offset in bytes and return 0 on success, nonzero
   otherwise (and then the file will be open for write).  For Zmodem,
   resync is always possible, but for SEAlink it is not.  Do not try
   any resyncs if remsize == 0.
*/

FILE *openfile(char*,time_t,off_t,off_t*,int(*)(off_t));
FILE *openfile(fname,remtime,remsize,resofs,resync)
char *fname;
time_t remtime;
off_t remsize;
off_t *resofs;
int (*resync)(off_t);
{
	char *opentype;
	char *p,x;
	char ctt[32];
	int rc,ncount;
	struct stat st;
	struct flock fl;
	char tmpfname[16];

	fl.l_type=F_WRLCK;
	fl.l_whence=0;
	fl.l_start=0L;
	fl.l_len=0L;
	strcpy(ctt,date(remtime));
	debug(11,"openfile(\"%s\",%s,%lu,...)",
		S(fname),S(ctt),(unsigned long)remsize);

	if ((fname == NULL) || (fname[0] == '\0'))
	{
		sprintf(tmpfname,"%08lx.pkt",(unsigned long)sequencer());
		fname=tmpfname;
	}
	if (infpath) free(infpath);
	infpath=xstrcpy(inbound);
	infpath=xstrcat(infpath,"/tmp/");
	infpath=xstrcat(infpath,fname);

	if ((strlen(fname) == 12) &&
	    (strspn(fname,"0123456789abcdefABCDEF") == 8) &&
	    (strcasecmp(fname+8,".req") == 0))
	{
		debug(12,"received wazoo freq file");
		isfreq=1;
	}
	else isfreq=0;

/*
	Renaming algorythm is as follows: start with the present name,
	increase the last character of the file name, jumping from 
	'9' to 'a', from 'z' to 'A', from 'Z' to '0'. If _all_ these 
	names are occupied, 
*/

	p=infpath+strlen(infpath)-1;
	x=*p;
	ncount=0;
	while (((rc=stat(infpath,&st)) == 0) &&
	       (remtime != st.st_mtime) &&
	       (ncount++ < 62))
	{
		if (x == '9') x='a';
		else if (x == 'z') x='A';
		else if (x == 'Z') x='0';
		else x++;
		*p=x;
	}
	if (isfreq || (ncount >= 62)) /* names exhausted */
	{
		rc=1;
		p=strrchr(infpath,'/');
		*p='\0';
		p=tempnam(infpath,",");
		free(infpath);
		infpath=xstrcpy(p); /* tempnam() returns pointer to a static
					buffer, right? */
	}
	*resofs=0L;
	opentype="w";
	if ((rc == 0) && (remsize != 0))
	{
		loginf("resyncing at offset %lu of \"%s\"",
			(unsigned long)st.st_size,infpath);
		if (resync(st.st_size) == 0)
		{
			opentype="a";
			*resofs=st.st_size;
		}
	}
	debug(11,"try fopen(\"%s\",\"%s\")",infpath,opentype);
	if ((infp=fopen(infpath,opentype)) == NULL)
	{
		logerr("$cannot open local file \"%s\" for \"%s\"",
			infpath,opentype);
		free(infpath);
		infpath=NULL;
		return NULL;
	}
	fl.l_pid=getpid();
	if (fcntl(fileno(infp),F_SETLK,&fl) != 0)
	{
		loginf("$cannot lock local file \"%s\"",infpath);
		fclose(infp);
		infp=NULL;
		free(infpath);
		infpath=NULL;
		return NULL;
	}
	intime=remtime;

	if (isfreq)
	{
		if (freqname) free(freqname);
		freqname=xstrcpy(infpath);
	}

	debug(11,"opened file \"%s\" for \"%s\", restart at %lu",
		infpath,opentype,(unsigned long)*resofs);
	return infp;
}

/*
   close file and if (success) { move it to the final location }
*/

int closefile(int);
int closefile(success)
int success;
{
	char *newpath,*p;
	int rc=0,ncount;
	char x;
	struct stat st;
	struct utimbuf ut;

	debug(11,"closefile(%d), for file \"%s\"",success,S(infpath));

	if ((infp == NULL) || (infpath == NULL))
	{
		logerr("internal error: try close unopened file!");
		return 1;
	}

	rc=fclose(infp);
	infp=NULL;

	if (rc == 0)
	{
		ut.actime=intime;
		ut.modtime=intime;
		if ((rc=utime(infpath,&ut)))
		{
			logerr("$utime failed");
		}
	}

	if (isfreq)
	{
		if ((rc != 0) || (!success))
		{
			loginf("removing unsuccessfuly received wazoo freq");
			unlink(freqname);
			free(freqname);
			freqname=NULL;
		}
		isfreq=0;
	}
	else if ((rc == 0) && success)
	{
		newpath=xstrcpy(inbound);
		newpath=xstrcat(newpath,strrchr(infpath,'/'));
		
		p=newpath+strlen(newpath)-1;
		x=*p;
		ncount=0;
		while (((rc=stat(newpath,&st)) == 0) &&
		       (ncount++ < 62))
		{
			if (x == '9') x='a';
			else if (x == 'z') x='A';
			else if (x == 'Z') x='0';
			else x++;
			*p=x;
		}
		if (ncount >= 62) /* names exhausted */
		{
			rc=1;
			p=strrchr(newpath,'/');
			*p='\0';
			p=tempnam(newpath,",");
			free(newpath);
			newpath=xstrcpy(p); /* tempnam() returns pointer to 
						a static buffer, right? */
		}

		debug(11,"moving \"%s\" -> \"%s\"",S(infpath),S(newpath));
		rc=rename(infpath,newpath);
		if (rc) logerr("$error renaming \"%s\" -> \"%s\"",
				S(infpath),S(newpath));

		free(newpath);
	}
	free(infpath);
	infpath=NULL;
	return rc;
}

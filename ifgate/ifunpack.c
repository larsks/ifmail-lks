#include <sys/types.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
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
#include "directory.h"
#ifdef HAS_SYSLOG
#include <syslog.h>
#endif
#include "mygetopt.h"
#include "xutil.h"
#include "lutil.h"
#include "config.h"
#include "version.h"
#include "trap.h"

#define UNPACK_FACTOR 300
#define TOSS_FACTOR 120
#ifndef NEWSSPOOL
#define NEWSSPOOL "/var/spool/news"
#endif

#define TMPNAME "TMP."
#define LCKNAME "LOCKFILE"

extern char* logname;

static int lockunpack(void);
static void ulockunpack(void);
static int unpack(char *);
static int toss(char *);
#if defined(HAS_STATFS) || defined(HAS_STATVFS)
static int checkspace(char *,char *,int);
static char *newsspool=NEWSSPOOL;
#endif
extern int f_lock(char *);
extern void funlock(int);
extern int execute(char *,char *,char *,char *,char *,char *);
extern char *unpacker(char *);

void usage(void)
{
	confusage("");
}

int main(argc,argv)
int argc;
char *argv[];
{
	int c;
	int rc=0,maxrc=0;
	int files=0,files_ok=0,packets=0,packets_ok=0;
	DIR *dp;
	struct dirent *de;

#if defined(HAS_SYSLOG) && defined(MAILLOG)
        logfacility=MAILLOG;
#endif

	setmyname(argv[0]);
	catch(myname);
	while ((c=getopt(argc,argv,"hx:I:")) != -1)
	if (confopt(c,optarg)) switch (c)
	{
		default:	usage(); exit(1);
	}

	if (readconfig())
	{
		fprintf(stderr,"Error getting configuration, aborting\n");
		exit(1);
	}

	if (chdir(protinbound) == -1)
	{
		logerr("$Error changing to directory %s",S(protinbound));
		exit(1);
	}

	if ((dp=opendir(protinbound)) == NULL)
	{
		logerr("$Error opening directory %s",S(protinbound));
		exit(1);
	}

	if (lockunpack()) exit(0);

	umask(066);

	while((de=readdir(dp)))
	if ((strlen(de->d_name) == 12) &&
	    ((strncasecmp(de->d_name+8,".su",3) == 0) ||
	     (strncasecmp(de->d_name+8,".mo",3) == 0) ||
	     (strncasecmp(de->d_name+8,".tu",3) == 0) ||
	     (strncasecmp(de->d_name+8,".we",3) == 0) ||
	     (strncasecmp(de->d_name+8,".th",3) == 0) ||
	     (strncasecmp(de->d_name+8,".fr",3) == 0) ||
	     (strncasecmp(de->d_name+8,".sa",3) == 0)))
	{
		files++;
#if defined(HAS_STATFS) | defined(HAS_STATVFS)
		if (checkspace(protinbound,de->d_name,UNPACK_FACTOR))
#endif
			if ((rc=unpack(de->d_name)) == 0) files_ok++;
			else logerr("Error unpacking file %s",de->d_name);
#if defined(HAS_STATFS) | defined(HAS_STATVFS)
		else loginf("Insufficient space to unpack file %s",de->d_name);
#endif
		if (rc > maxrc) maxrc=rc;
	}

	rewinddir(dp);

	while((de=readdir(dp)))
	if ((strlen(de->d_name) == 12) &&
	    (strncasecmp(de->d_name+8,".pkt",4) == 0))
	{
		packets++;
#if defined(HAS_STATFS) | defined(HAS_STATVFS)
		if (checkspace(newsspool,de->d_name,TOSS_FACTOR))
#endif
			if ((rc=toss(de->d_name)) == 0) packets_ok++;
			else logerr("Error tossing packet %s",de->d_name);
#if defined(HAS_STATFS) | defined(HAS_STATVFS)
		else loginf("Insufficient space to toss packet %s",de->d_name);
#endif
		if (rc > maxrc) maxrc=rc;
	}

	closedir(dp);

	if (files || packets)
		loginf("processed %d of %d files, %d of %d packets, rc=%d",
			files_ok,files,packets_ok,packets,maxrc);

	ulockunpack();

	return maxrc;
}

int unpack(fn)
char *fn;
{
	char newname[16];
	char *cmd;
	int rc,ld;

	if ((cmd=unpacker(fn)) == NULL) return 1;
	if ((ld=f_lock(fn)) == -1) return 1;
	rc=execute(cmd,fn,(char *)NULL,"/dev/null",logname,logname);
	if (rc == 0) unlink(fn);
	else
	{
		strncpy(newname,fn,sizeof(newname)-1);
		strcpy(newname+8,".bad");
		rename(fn,newname);
	}
	funlock(ld);
	return rc;
}

int toss(fn)
char *fn;
{
	int rc,ld;
	char newname[16];
	char *cmd,tmpb[32],*p;
	int i;

	if ((ld=f_lock(fn)) == -1) return 1;
	p=tmpb;
	*p='\0';
	for (i=0;i<26;i++)
		if (verbose & (1<<i))
			*p++='a'+i;
	*p='\0';
	cmd=xstrcpy(iftoss);
	if (tmpb[0])
	{
		cmd=xstrcat(cmd," -x ");
		cmd=xstrcat(cmd,tmpb);
	}
	cmd=xstrcat(cmd," -I ");
	cmd=xstrcat(cmd,configname);
	rc=execute(cmd,(char *)NULL,(char *)NULL,fn,logname,logname);
	free(cmd);
	if (rc == 0) unlink(fn);
	else
	{
		strncpy(newname,fn,sizeof(newname)-1);
		strcpy(newname+8,".bad");
		rename(fn,newname);
	}
	funlock(ld);
	return rc;
}

static char lockfile[PATH_MAX];

int lockunpack(void)
{
	char tmpfile[PATH_MAX];
	FILE *fp;
	pid_t oldpid;

	strncpy(tmpfile,inbound,sizeof(tmpfile)-strlen(LCKNAME)-16);
	strcat(tmpfile,"/");
	strcpy(lockfile,tmpfile);
	sprintf(tmpfile+strlen(tmpfile),"%s%u",TMPNAME,getpid());
	sprintf(lockfile+strlen(lockfile),"%s",LCKNAME);
	if ((fp=fopen(tmpfile,"w")) == NULL) {
		logerr("$cannot open lockfile \"%s\"",tmpfile);
		return 1;
	}
	fprintf(fp,"%10u\n",getpid());
	fclose(fp);
	while (1) {
		if (link(tmpfile,lockfile) == 0) {
			unlink(tmpfile);
			return 0;
		}
		if ((fp=fopen(lockfile,"r")) == NULL) {
			logerr("$cannot open lockfile \"%s\"",tmpfile);
			unlink(tmpfile);
			return 1;
		}
		if (fscanf(fp,"%u",&oldpid) != 1) {
			logerr("$cannot read old pid from \"%s\"",tmpfile);
			fclose(fp);
			unlink(tmpfile);
			return 1;
		}
		fclose(fp);
		if (kill(oldpid,0) == -1) {
			if (errno == ESRCH) {
				loginf("stale lock found for pid %u",oldpid);
				unlink(lockfile);
				/* no return, try lock again */
			} else {
				logerr("$kill for %u failed",oldpid);
				unlink(tmpfile);
				return 1;
			}
		} else {
			loginf("unpack already running, pid=%u",oldpid);
			unlink(tmpfile);
			return 1;
		}
	}
}

void ulockunpack(void)
{
	if (lockfile) (void)unlink(lockfile);
}

#if defined(HAS_STATFS) | defined(HAS_STATVFS)
int checkspace(dir,fn,factor)
char *dir,*fn;
int factor;
{
	struct stat st;

#ifdef HAS_STATVFS
	struct statvfs sfs;

	if ((stat(fn,&st) != 0) || (statvfs(dir,&sfs) != 0))
#else
	struct statfs sfs;

#ifdef SCO_STYLE_STATFS
	if ((stat(fn,&st) != 0) || (statfs(dir,&sfs,sizeof(sfs),0) != 0))
#else
	if ((stat(fn,&st) != 0) || (statfs(dir,&sfs) != 0))
#endif
#endif
	{
		logerr("$cannot stat \"%s\" or statfs \"%s\", assume enough space",
			S(fn),S(dir));
		return 1;
	}
	if ((((st.st_size/sfs.f_bsize+1)*factor)/100L) > sfs.f_bfree)
	{
		loginf("Only %lu %lu-byte blocks left on device where %s is located",
			sfs.f_bfree,sfs.f_bsize,S(dir));
		return 0;
	}
	return 1;
}
#endif

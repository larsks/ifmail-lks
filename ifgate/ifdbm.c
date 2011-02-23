#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#ifdef HAS_NDBM_H
#include <fcntl.h>
#include <ndbm.h>
#else
#include <dbm.h>
#endif
#include "lutil.h"
#include "config.h"

static int opened=0;
#ifdef HAS_NDBM_H
static DBM *alias_db = NULL;
#endif

static int init(void)
{
#ifndef HAS_BSD_DB
	char buf[128];
	struct stat stbuf;
	FILE *fp;
#endif
	int tries;

	if (database == NULL) return -1;
	if (opened == -1) return -1;
	if (opened) return 0;
#ifndef HAS_BSD_DB
	sprintf(buf,"%s.dir",database);
	if (stat(buf,&stbuf) != 0)
	{
		sprintf(buf,"%s.dir",database);
		fp=fopen(buf,"a");
		if (fp) fclose(fp);
		sprintf(buf,"%s.pag",database);
		fp=fopen(buf,"a");
		if (fp) fclose(fp);
	}
#endif
	for (tries=0;(1);tries++)
	{
#ifdef HAS_NDBM_H
		if ((alias_db=dbm_open(database,O_RDWR | O_CREAT,0600)) != NULL)
#else
		if (dbminit(database) == 0)
#endif
		{
			opened = 1;
			return 0;
		}
		else if ((tries > 5) || (errno != EAGAIN))
		{
			logerr("$could not open alias database");
			opened = -1;
			return -1;
		}
		else
			sleep(2);
	}
}

int registrate(freename,address)
char *freename,*address;
{
	datum key,val;
	char buf[128],*p,*q;
	int first;

	if (init()) return 1;

	strncpy(buf,freename,sizeof(buf)-1);
	first=1;
	for (p=buf,q=buf;*p;p++) switch (*p)
	{
	case '.':	*p=' '; /* fallthrough */
	case ' ':	if (first)
			{
				*(q++)=*p;
				first=0;
			}
			break;
	default:	*(q++)=*p;
			first=1;
			break;
	}
	*q='\0';
	debug(6,"registrate \"%s\" \"%s\"",S(buf),S(address));
	key.dptr=buf;
	key.dsize=strlen(buf);
#ifdef HAS_NDBM_H
	val=dbm_fetch(alias_db,key);
#else
	val=fetch(key);
#endif
	if (val.dptr) return 0; /* already present */
	else
	{
		val.dptr=address;
		val.dsize=strlen(address);
#ifdef HAS_NDBM_H
		if (dbm_store(alias_db,key,val,0) < 0)
#else
		if (store(key,val) != 0)
#endif
		{
			logerr("$cannot store: \"%s\" \"%s\"",
				S(buf),S(address));
			return 1;
		}
		else
		{
			loginf("registered \"%s\" as \"%s\"",S(buf),S(address));
			return 0;
		}
	}
}

char *lookup(freename)
char *freename;
{
	datum key,val;
	static char buf[128],*p,*q;
	int sz,first;

	if (init()) return NULL;

	strncpy(buf,freename,sizeof(buf)-1);
	first=1;
	for (p=buf,q=buf;*p;p++) switch (*p)
	{
	case '.':	*p=' '; /* fallthrough */
	case ' ':	if (first)
			{
				*(q++)=*p;
				first=0;
			}
			break;
	default:	*(q++)=*p;
			first=1;
			break;
	}
	*q='\0';
	debug(6,"lookup \"%s\"",S(freename));
	key.dptr=buf;
	key.dsize=strlen(buf);
#ifdef HAS_NDBM_H
	val=dbm_fetch(alias_db,key);
#else
	val=fetch(key);
#endif
	if (val.dptr)
	{
		sz=val.dsize;
		if (sz > (sizeof(buf)-1)) sz=sizeof(buf)-1;
		strncpy(buf,val.dptr,sz);
		buf[sz]='\0';
		debug(6,"found: \"%s\"",buf);
		return(buf);
	}
	else return NULL;
}

void close_alias_db(void)
{
	if (opened != 1) return;
#ifndef DONT_HAVE_DBMCLOSE
#ifdef HAS_NDBM_H
	dbm_close(alias_db);
#else
	dbmclose();
#endif
#endif
	opened=0;
}

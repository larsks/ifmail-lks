#ifdef HAS_NDBM_H
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <ndbm.h>
#include <fcntl.h>
#include "lutil.h"
#include "xutil.h"
#include "config.h"
#include "ftn.h"
#include "rfcaddr.h"

static DBM *id_db = NULL;
static int opened = 0;

static int fillidb(void);

static int init(void)
{
	int needbuild;
	struct stat stbuf1;
	char buf[128];
	struct stat stbuf2;
#ifndef HAS_BSD_DB
	FILE *fp;
#endif

	if ((msgidbm == NULL) || (newslog == NULL)) return -1;
	if (opened == -1) return -1;
	if (stat(newslog,&stbuf1) != 0) return -1;
	if (opened) return 0;
	needbuild=0;
#ifdef HAS_BSD_DB
	sprintf(buf,"%s.db",msgidbm);
#else
	sprintf(buf,"%s.dir",msgidbm);
#endif
	if ((stat(buf,&stbuf2) != 0) ||
	    (stbuf1.st_mtime > stbuf2.st_mtime))
	{
		loginf("SEEN-BY database rebuild requested");
		needbuild=1;
#ifdef HAS_BSD_DB
		sprintf(buf,"%s.db",msgidbm);
		unlink(buf);
#else
		sprintf(buf,"%s.dir",msgidbm);
		if ((fp=fopen(buf,"w"))) fclose(fp);
		sprintf(buf,"%s.pag",msgidbm);
		if ((fp=fopen(buf,"w"))) fclose(fp);
#endif
	}
	if ((id_db=dbm_open(msgidbm,O_RDWR | O_CREAT,0600)) == NULL)
	{
		opened = -1;
		return -1;
	}
	opened = 1;

	if (needbuild) return fillidb();
	return 0;
}

static int fillidb(void)
{
	datum key,val;
	char buf[128],*line=NULL,se[16],*seen=NULL,*p,*q,*pe,*id;
	faddr *fa = NULL;
	FILE *fp;
	int n=0;

	if ((fp=fopen(newslog,"r")) == NULL) return -1;

	while (!feof(fp))
	{
		while (((line == NULL) || (line[strlen(line)-1] != '\n')) &&
		       (fgets(buf,sizeof(buf)-1,fp)))
			line=xstrcat(line,buf);
		if (line == NULL) goto nextline;
		pe=line+strlen(line);
		strtok(line," \t\n");	/* Month */
		strtok(NULL," \t\n");	/* Date */
		strtok(NULL," \t\n");	/* Time */
		if ((p=strtok(NULL," \t\n")) != NULL)
		{
			if (*p == '+') /* INN style log */
			{
				p=strtok(NULL," \t\n"); /* skip source host */
			}
			else /* maybe cnews style log? */
			{
				if (((p=strtok(NULL," \t\n")) == NULL) ||
				    (*p != '+')) goto nextline;
			}
		}

		debug(8,"good log entry for %s",p);

		if ((id=strtok(NULL," \t\n")) == NULL) goto nextline;
		/* some witchkraft with char *q,*pe is needed because */
		/* otherwise strtok called from within parsefaddr will */
		/* interfere with strtok in the while cycle */
		q=NULL;
		while ((p=strtok(q," \t\n")))
		{
			if ((q=p+strlen(p)+1) >= pe) q=NULL;
			debug(8,"try parse %s",p);
			if ((fa=parsefaddr(p)))
			{
				sprintf(se,"%s ",
					ascfnode(fa,0x06));
				seen=xstrcat(seen,se);
				debug(8,"new seen: \"%s\"",seen);
				tidy_faddr(fa);
			}
			else debug(8,"unparsable: %s",addrerrstr(addrerror));
		}
		if (seen)
		{
			key.dptr=id;
			key.dsize=strlen(id);
			val.dptr=seen;
			val.dsize=strlen(seen);
			*(seen+strlen(seen)-1) = '\0';
			if (dbm_store(id_db,key,val,0) < 0)
				logerr("$cannot store: \"%s\" \"%s\"",id,seen);
			else debug(8,"seen-by for \"%s\" is \"%s\"",id,seen);
			n++;
		}
	nextline:
		if (seen) free(seen);
		seen=NULL;
		if (line) free(line);
		line=NULL;
	}
	loginf("SEEN-BY database now contains %d records",n);
	return 0;
}

char *idlookup(msgid)
char *msgid;
{
	datum key,val;

	if (init()) return "";

	debug(8,"idlookup \"%s\"",S(msgid));
	key.dptr=msgid;
	key.dsize=strlen(msgid);
	val=dbm_fetch(id_db,key);
	if (val.dptr) return val.dptr;
	else return "";
}

void close_id_db(void)
{
	if (opened != 1) return;
#ifndef DONT_HAVE_CLOSEDBM
#ifdef HAS_NDBM_H
	dbm_close(id_db);
#endif
#endif
	opened=0;
}
#endif

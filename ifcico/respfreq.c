#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#ifdef HAS_REGEX_H
#include <regex.h>
#else
char *re_comp(char*);
int re_exec(char*);
#endif
#include "directory.h"
#include "session.h"
#include "xutil.h"
#include "lutil.h"
#include "config.h"
#ifndef NOFREQREPORT
#ifdef HAS_STDARG_H
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include "ftnmsg.h"
#endif
#include "version.h"

#ifndef S_ISDIR
#define S_ISDIR(st_mode)       (((st_mode) & S_IFMT) == S_IFDIR)
#endif
#ifndef S_ISREG
#define S_ISREG(st_mode)       (((st_mode) & S_IFMT) == S_IFREG)
#endif

#define KFS 2

extern char *freqname;
extern char *logname;
extern int execsh(char*,char*,char*,char*);
extern unsigned long atoul(char*);
extern unsigned INT32 sequencer(void);

static file_list *respfreq(char*,char*,char*);
static file_list *respmagic(char*);
static file_list *resplist(char*);

#ifndef NOFREQREPORT
static void attach_report(file_list**);
static void add_report(char *fmt,...);
static char *report_text=NULL;
static unsigned long report_total=0L;
#endif

file_list *respond_wazoo(char*);
file_list *respond_wazoo(fn)
char *fn;
{
	char buf[256];
	char *nm,*pw,*dt,*p;
	file_list *fl=NULL,**tmpl;
	FILE *fp;

	if (freqname == NULL) return NULL;

	if ((fp=fopen(freqname,"r")) == NULL)
	{
		logerr("$cannot open received wazoo freq \"%s\"",freqname);
		unlink(freqname);
		free(freqname);
		freqname=NULL;
		return NULL;
	}

	tmpl=&fl;
	while (fgets(buf,sizeof(buf)-1,fp))
	{
		nm=NULL;
		pw=NULL;
		dt=NULL;
		p=strtok(buf," \n\r");
		if ((p == NULL) || (*p == '\0')) continue;
		nm=p;
		p=strtok(NULL," \n\r");
		if (p && (*p == '!')) pw=p+1;
		else if (p && ((*p == '+') || (*p == '-'))) dt=p;
		p=strtok(NULL," \n\r");
		if (p && (*p == '!')) pw=p+1;
		else if (p && ((*p == '+') || (*p == '-'))) dt=p;
		*tmpl=respfreq(nm,pw,dt);
		while (*tmpl) tmpl=&((*tmpl)->next);
	}

	fclose(fp);
	unlink(freqname);
	free(freqname);
	freqname=NULL;
	for (tmpl=&fl;*tmpl;tmpl=&((*tmpl)->next))
	{
		debug(12,"resplist: %s",S((*tmpl)->local));
	}
#ifndef NOFREQREPORT
	attach_report(&fl);
#endif
	return fl;
}

file_list *respond_bark(char*);
file_list *respond_bark(buf)
char *buf;
{
	char *nm,*pw,*dt,*p;
	file_list *fl;

	nm=buf;
	pw="";
	dt="0";
	while (isspace(*nm)) nm++;
	for (p=nm;*p && (!isspace(*p));p++);
	if (*p)
	{
		*p++='\0';
		dt=p;
		while (isspace(*dt)) dt++;
		for (p=dt;*p && (!isspace(*p));p++);
		if (*p)
		{
			*p++='\0';
			pw=p;
			while (isspace(*pw)) pw++;
			for (p=pw;*p && (!isspace(*p));p++);
			*p='\0';
		}
	}
	fl=respfreq(nm,pw,dt);
#ifndef NOFREQREPORT
	attach_report(&fl);
#endif
	return fl;
}

file_list *respfreq(nm,pw,dt)
char *nm,*pw,*dt;
{
	file_list *fl=NULL;
	DIR *dp;
	struct dirent *de;
	struct stat st;
	char mask[256],*p,*q;
	char *tnm,*tdir;
	time_t upd=0L;
	int newer=1,pass;

	loginf("remote requested \"%s\" (update %s, password \"%s\")",
		S(nm),S(dt),S(pw));

	if (localoptions & NOFREQS)
	{
		loginf("File requests forbidden");
#ifndef NOFREQREPORT
		add_report("ER: \"%s\" denied: file requests forbidden",
			nm);
#endif
		return NULL;
	}

	if (dt)
	{
		if (*dt == '+')
		{
			newer=1;
			dt++;
		}
		else if (*dt == '-')
		{
			newer=0;
			dt++;
		}
		else newer=1;
		upd=atoul(dt);
	}
	if (magic && !strchr(nm,'/'))
	{
		tnm=xstrcpy(magic);
		tnm=xstrcat(tnm,"/");
		tnm=xstrcat(tnm,nm);
		if ((stat(tnm,&st) == 0) &&
		    (S_ISREG(st.st_mode)))
		{
			if (access(tnm,X_OK) == 0)
			{
				loginf("Execute request");
				return respmagic(tnm);
				/* respmagic will free(tnm) */
			}
			else if (access(tnm,R_OK) == 0)
			{
				loginf("Reference request");
				return resplist(tnm);
				/* resplist will free(tnm) */
			}
			else free(tnm);
		}
		else free(tnm);
	}

	if (pubdir == NULL)
	{
#ifndef NOFREQREPORT
		add_report("ER: \"%s\" failed: no requestable files available",
			nm);
#endif
		return NULL;
	}

#ifndef NOFREQREPORT
	add_report("RQ: Regular file \"%s\"",nm);
#endif

	tdir=xstrcpy(pubdir);
	if ((p=strrchr(nm,'/')))
	{
		*p++='\0';
		tdir=xstrcat(tdir,"/");
		tdir=xstrcat(tdir,nm);
		if (strstr(tdir,"/../"))
		{
#ifndef NOFREQREPORT
			add_report("WN: attempt to leave restricted directory");
#endif
			loginf("attempt to leave restricted directory: \"%s\"",tdir);
			free(tdir);
			tdir=xstrcpy(pubdir);
			tdir=xstrcat(tdir,"/");
		}
	}
	else
		p=nm;

	q=mask;
	*q++='^';
	while ((*p) && (q < (mask+sizeof(mask)-4)))
	{
		switch (*p)
		{
		case '\\':	*q++='\\'; *q++='\\'; break;
		case '?':	*q++='.'; break;
		case '.':	*q++='\\'; *q++='.'; break;
		case '+':	*q++='\\'; *q++='+'; break;
		case '*':	*q++='.'; *q++='*'; break;
		default:	*q++=*p; break;
		}
		p++;
	}
	*q++='$';
	*q='\0';

	if ((dp=opendir(tdir)) == NULL)
	{
		for (p=tdir;*p;p++) *p=tolower(*p);
		dp=opendir(tdir);
	}
	if (dp == NULL)
	{
		logerr("$cannot opendir \"%s\"",tdir);
#ifndef NOFREQREPORT
		add_report("ER: Could not open directory \"%s\"",tdir);
#endif
		free(tdir);
		return NULL;
	}

	for (pass=0;(pass < 2) && (fl == NULL);pass++)
	{
		if (pass > 0)
		{
			for (p=mask;*p;p++) *p=tolower(*p);
			rewinddir(dp);
		}
		debug(12,"try search mask: \"%s\"",mask);
		re_comp(mask);

		while ((de=readdir(dp)))
		if (re_exec(de->d_name))
		{
			debug(12,"matching file \"%s\"",de->d_name);
			tnm=xstrcpy(tdir);
			tnm=xstrcat(tnm,"/");
			tnm=xstrcat(tnm,de->d_name);

			if ((stat(tnm,&st) == 0) &&
			    (S_ISREG(st.st_mode)) &&
			    (access(tnm,R_OK) == 0) &&
			    ((upd == 0L) ||
			     ((newer) && (st.st_mtime > upd)) ||
			     ((!newer) && (st.st_mtime <= upd))))
			{
#ifndef NOFREQREPORT
				report_total+=st.st_size;
				add_report("OK: Sending \"%s\" (%lu bytes)",
					de->d_name,(unsigned long)st.st_size);
#endif
				add_list(&fl,tnm,de->d_name,0,0L,NULL,1);
			}
			else
			{
#ifndef NOFREQREPORT
				add_report("ER: Not sending \"%s\" (could not access or does not meet update condition)",
					de->d_name);
#endif
			}
			free(tnm);
		}
	}

	if (fl == NULL)
	{
#ifndef NOFREQREPORT
		add_report("ER: No matching files found in \"%s\"",tdir);
#endif
	}

	free(tdir);
	closedir(dp);
	return fl;
}

#define MAXRECURSE 5
static int recurse=0;

file_list *resplist(listfn) /* must free(listfn) before exit */
char *listfn;
{
	FILE *fp;
	char buf[256],*p;
	file_list *fl=NULL,**pfl;

	if (++recurse > MAXRECURSE)
	{
		logerr("Excessive recursion in file lists for \"%s\"",
			S(listfn));
#ifndef NOFREQREPORT
		add_report("ER: Exessive recursion for reference \"%s\", contact sysop",
			listfn);
#endif
		recurse=0;
		free(listfn);
		return NULL;
	}

	pfl=&fl;
	if ((fp=fopen(listfn,"r")) == NULL)
	{
		logerr("$cannot open file list \"%s\"",listfn);
#ifndef NOFREQREPORT
		add_report("ER: Could not open reference file \"%s\", contact sysop",
			listfn);
#endif
		free(listfn);
		recurse--;
		return NULL;
	}
#ifndef NOFREQREPORT
	add_report("RQ: Expanding reference \"%s\"",listfn);
#endif
	while(fgets(buf,sizeof(buf)-1,fp))
	{
		if ((p=strchr(buf,'#'))) *p='\0';
		if ((p=strtok(buf," \t\n\r")))
		{
			*pfl=respfreq(p,NULL,NULL);
			while (*pfl) pfl=&((*pfl)->next);
		}
	}
	fclose(fp);

	free(listfn);
	recurse--;
	return fl;
}

file_list *respmagic(cmd) /* must free(cmd) before exit */
char *cmd;
{
	struct stat st;
	char cmdbuf[256];
	char tmpfn[L_tmpnam];
	char remname[32],*p,*q,*z;
	int escaped;
	file_list *fl=NULL;

	debug(12,"respond to \"magic\" file request \"%s\"",S(cmd));
#ifndef NOFREQREPORT
	add_report("RQ: Magic \"%s\"",cmd);
#endif
	(void)tmpnam(tmpfn);
	if ((p=strrchr(cmd,'/'))) p++;
	else p=cmd;
	strncpy(remname,p,sizeof(remname)-1);
	remname[sizeof(remname)-1]='\0';
	if (remote->addr->name == NULL) remote->addr->name=xstrcpy("Sysop");
	strncpy(cmdbuf,cmd,sizeof(cmdbuf)-2);
	cmdbuf[sizeof(cmdbuf)-2]='\0';
	q=cmdbuf+strlen(cmdbuf);
	z=cmdbuf+sizeof(cmdbuf)-2;
	*q++=' ';
	escaped=0;
	for (p=ascfnode(remote->addr,0x7f); *p && (q < z); p++)
	{
		if (escaped)
		{
			escaped=0;
		}
		else switch (*p)
		{
		case '\\':	escaped=1; break;
		case '\'':
		case '`':
		case '"':
		case '(':
		case ')':
		case '<':
		case '>':
		case '|':
		case ';':
		case '$':
				*q++='\\'; break;
		}
		*q++=*p;
	}
	*q++='\0';
	if (execsh(cmdbuf,"/dev/null",tmpfn,logname))
	{
		logerr("error executing magic file request");
#ifndef NOFREQREPORT
		add_report("ER: Magic command execution failed");
#endif
		unlink(tmpfn);
	}
	else
	{
		if (stat(tmpfn,&st) == 0)
		{
			add_list(&fl,tmpfn,remname,KFS,0L,NULL,1);
#ifndef NOFREQREPORT
			report_total+=st.st_size;
			add_report("OK: Sending magic output (%lu bytes)",
				(unsigned long)st.st_size);
#endif
			debug(12,"magic resp list: \"%s\" -> \"%s\"",
				S(fl->local),S(fl->remote));
		}
		else
		{
			logerr("$cannot stat() magic stdout \"%s\"",tmpfn);
#ifndef NOFREQREPORT
			add_report("ER: Could not get magic command output, contact sysop");
#endif
		}
	}
	free(cmd);
	return fl;
}

#ifndef NOFREQREPORT

static void attach_report(fl)
file_list **fl;
{
	FILE *fp;
	char tmpfn[L_tmpnam];
	char remname[14];
	long zeroes=0L;
	ftnmsg fmsg;
	char *svname;

	if (report_text == NULL)
	{
		logerr("Empty FREQ report");
		add_report("ER: empty request report, contact sysop");
	}

	add_report("\r\n\
                     Total to send: %lu bytes\r\n\r\n\
--- ifcico v.%s\r\n\
",report_total,version);

	debug(12,"\nFREQ report is:\n====\n%s\n====",report_text);

	(void)tmpnam(tmpfn);
	if ((fp=fopen(tmpfn,"w")))
	{
		fmsg.flags=FLG_PVT|FLG_K_S;
		fmsg.from=bestaka_s(remote->addr);
		svname=fmsg.from->name;
		fmsg.from->name="ifcico FREQ processor";
		debug(12,"from: %s",ascfnode(fmsg.from,0x7f));
		fmsg.to=remote->addr;
		debug(12,"to:   %s",ascfnode(fmsg.to,0x7f));
		fmsg.date=time((time_t*)NULL);
		fmsg.subj="File request status report";
		fmsg.msgid_s=NULL;
		fmsg.msgid_a=NULL;
		fmsg.reply_s=NULL;
		fmsg.reply_a=NULL;
		fmsg.origin=NULL;
		fmsg.area=NULL;
		(void)ftnmsghdr(&fmsg,fp,NULL,'f');
		fwrite(report_text,1,strlen(report_text),fp);
		fwrite(&zeroes,1,3,fp);
		fclose(fp);
		sprintf(remname,"%08lX.PKT",(unsigned long)sequencer());
		add_list(fl,tmpfn,remname,KFS,0L,NULL,0);
		fmsg.from->name=svname;
	}
	else
	{
		logerr("$cannot open temp file \"%s\"",S(tmpfn));
	}

	report_total=0L;
	free(report_text);
	report_text=NULL;
}

#ifdef HAS_STDARG_H
static void add_report(char *fmt, ...)
{
	va_list	args;
#else
static void add_report(va_alist)
va_dcl
{
	va_list	args;
	char	*fmt;
#endif
	char buf[1024];

#ifdef HAS_STDARG_H
	va_start(args, fmt);
#else
	va_start(args);
	fmt=va_arg(args, char*);
#endif

	if (report_text == NULL)
	{
		sprintf(buf,
"                    Status of file request\r\n\
                    ======================\r\n\r\n\
                    Received By: %s\r\n\
",
			ascfnode(bestaka_s(remote->addr),0x1f));
		sprintf(buf+strlen(buf),
"                           From: %s\r\n\
                             On: %s\r\n\r\n\
",
			ascfnode(remote->addr,0x1f),
			date(0L));
		report_text=xstrcat(report_text,buf);
	}
	vsprintf(buf,fmt,args);
	strcat(buf,"\r\n");
	report_text=xstrcat(report_text,buf);
}

#endif

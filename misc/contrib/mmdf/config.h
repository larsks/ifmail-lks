#ifndef _CONFIG_H
#define _CONFIG_H

#include "ftn.h"

#ifndef AREA_LIST_TYPE
#define AREA_LIST_TYPE
typedef struct _area_list {
	struct _area_list *next;
	char *name;
} area_list;
#endif

typedef struct _modem_string {
	struct _modem_string *next;
	char *line;
	char *expr;
} modem_string;

typedef struct _dom_trans {
	struct _dom_trans *next;
	char *ftndom;
	char *intdom;
} dom_trans;

extern char *configname;
extern char *nlbase;

extern fa_list *whoami;
extern fa_list *pwlist;
extern fa_list *nllist;

extern dom_trans *domtrans;

extern modem_string *modemport;
extern modem_string *phonetrans;
extern modem_string *modemreset;
extern modem_string *modemdial;
extern modem_string *modemhangup;
extern modem_string *modemok;
extern modem_string *modemconnect;
extern modem_string *modemerror;
extern modem_string *options;

extern area_list *badgroups;

extern long configverbose;
extern long configtime;
extern long maxfsize;
extern long speed;

extern char intab[];
extern char outtab[];

extern char *name;
extern char *location;
extern char *sysop;
extern char *phone;
extern char *flags;
extern char *inbound;
extern char *norminbound;
extern char *listinbound;
extern char *protinbound;
extern char *outbound;
extern char *database;
extern char *aliasfile;
extern char *myfqdn;
extern char *sequence;
extern char *sendmail;
extern char *rnews;
extern char *iftoss;
extern char *packer;
extern char *unzip;
extern char *unarj;
extern char *unlzh;
extern char *unarc;
extern char *unzoo;
extern char *areafile;
extern char *newslog;
extern char *msgidbm;
extern char *public;
extern char *magic;
extern char *debugfile;
extern char *routefile;

int readconfig(void);
int confopt(int,char*);
void confusage(char*);
#endif

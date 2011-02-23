#ifndef NLINDEX_H
#define NLINDEX_H

#include "nodelist.h"

#define INDEX "index"

#ifdef HAS_NDBM_H
#include <ndbm.h>
extern DBM *nldb;
#else
#include <dbm.h>
#endif
extern int openstatus;

extern struct _nodelist {
	char *domain;
	FILE *fp;
} *nodevector;

struct _ixentry {
	unsigned short zone;
	unsigned short net;
	unsigned short node;
	unsigned short point;
};
struct _loc {
	off_t off;
	unsigned short nlnum;
	unsigned short hub;
};

extern struct _pkey {
	char *key;
	unsigned char type;
	unsigned char pflag;
} pkey[];

extern struct _fkey fkey[];

extern int initnl(void);

#endif

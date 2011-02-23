#ifndef NODELIST_H
#define NODELIST_H

#include "ftn.h"

#define MAXUFLAGS 16

typedef struct _node {
	faddr addr;
	unsigned short hub;
	unsigned char type;
	unsigned char pflag;
	char *name;
	char *location;
	char *sysop;
	char *phone;
	unsigned speed;
	unsigned long flags;
	char *uflags[MAXUFLAGS];
} node;

/* type values */
#define NL_NONE		0
#define NL_ZONE		1
#define NL_NET		2
#define NL_NODE		3
#define NL_POINT	4

/* pflag values */
#define NL_DOWN		0x01
#define NL_HOLD		0x02
#define NL_DUMMY	0x04
#define NL_HUB		0x08
#define NL_PVT		0x10
#define NL_REGION	0x20

/* flag values */
#define CM	0x00000001L
#define MO	0x00000002L
#define LO	0x00000004L
#define V21	0x00000008L
#define V22	0x00000010L
#define V29	0x00000020L
#define V32	0x00000040L
#define V32B	0x00000080L
#define V33	0x00000100L
#define V34	0x00000200L
#define V42	0x00000400L
#define V42B	0x00000800L
#define MNP	0x00001000L
#define H96	0x00002000L
#define HST	0x00004000L
#define H14	0x00008000L
#define H16	0x00010000L
#define MAX	0x00020000L
#define PEP	0x00040000L
#define CSP	0x00080000L
#define ZYX	0x00100000L

#define MN	0x08000000L

#define RQMODE	0xf0000000L
#define RQ_BR	0x10000000L
#define RQ_BU	0x20000000L
#define RQ_WR	0x40000000L
#define RQ_WU	0x80000000L
#define XA	(RQ_BR | RQ_BU | RQ_WR | RQ_WU)
#define XB	(RQ_BR | RQ_BU | RQ_WR        )
#define XC	(RQ_BR |         RQ_WR | RQ_WU)
#define XP	(RQ_BR | RQ_BU                )
#define XR	(RQ_BR |         RQ_WR        )
#define XW	(                RQ_WR        )
#define XX	(                RQ_WR | RQ_WU)

extern node *getnlent();
extern void *endnlent();
extern int flagexp();

extern struct _fkey {
	char *key;
	unsigned long flag;
} fkey[];

#endif

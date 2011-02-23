#ifndef SESSION_H
#define SESSION_H

#include <stdio.h>
#include <sys/types.h>
#include "ftn.h"
#include "nodelist.h"

#define SESSION_UNKNOWN 0
#define SESSION_FTSC 1
#define SESSION_YOOHOO 2
#define SESSION_EMSI 3

#define SESSION_SLAVE 0
#define SESSION_MASTER 1

extern node *nlent;
extern fa_list *remote;

typedef struct _file_list {
	struct _file_list *next;
	char *local;
	char *remote;
	int disposition;
	FILE *flofp;
	off_t floff;
} file_list;

#define HOLD_MAIL "h"
#define NONHOLD_MAIL "co"
#define ALL_MAIL "coh"

extern int session_flags;
extern int remote_flags;
#define FTSC_XMODEM_CRC  1 /* xmodem-crc */
#define FTSC_XMODEM_RES  2 /* sealink-resync */
#define FTSC_XMODEM_SLO  4 /* sealink-overdrive */
#define FTSC_XMODEM_XOF  8 /* xoff flow control, aka macflow */
#define WAZOO_ZMODEM_ZAP 1 /* ZedZap allowed */

#define SESSION_WAZOO 0x8000 /* WaZOO type file requests */
#define SESSION_BARK  0x4000 /* bark type file requests */
#define SESSION_IFNA  0x2000 /* DietIFNA transfer from Yoohoo session */
#define SESSION_FNC   0x1000 /* Filename conversion sending files */

#define SESSION_TCP   0x0800 /* Established over TCP/IP link */

extern int localoptions;
#define NOCALL   0x0001
#define NOHOLD   0x0002
#define NOPUA    0x0004
#define NOWAZOO  0x0008
#define NOEMSI   0x0010
#define NOFREQS  0x0020
#define NOZMODEM 0x0040
#define NOZEDZAP 0x0080
#define NOJANUS  0x0100
#define NOHYDRA  0x0200
#define NOTCP    0x0400

extern int session(faddr*,node*,int,int,char*);
extern int tx_ftsc(void);
extern int tx_yoohoo(void);
extern int tx_emsi(char*);
extern int rx_ftsc(void);
extern int rx_yoohoo(void);
extern int rx_emsi(char*);
extern file_list *create_filelist(fa_list*,char*,int);
void add_list(file_list**,char*,char*,int,off_t,FILE*,int);
extern void tidy_filelist(file_list*,int);
extern void execute_disposition(file_list*);

#endif

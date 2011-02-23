#define PRODCODE 0xfe	/* product code for ifcico */

#ifndef EMSI_H
#define EMSI_H

#define PUA 0x0001
#define PUP 0x0002
#define NPU 0x0004
#define HAT 0x0008
#define HXT 0x0010
#define HRQ 0x0020

extern int emsi_local_lcodes;
extern int emsi_remote_lcodes;

#define DZA 0x0001
#define ZAP 0x0002
#define ZMO 0x0004
#define JAN 0x0008
#define KER 0x0010
#define HYD 0x0020
#define TCP 0x0040

extern int emsi_local_protos;
extern int emsi_remote_protos;

#define NRQ 0x0002
#define ARC 0x0004
#define XMA 0x0008
#define FNC 0x0010
#define CHT 0x0020
#define SLK 0x0040

extern int emsi_local_opts;
extern int emsi_remote_opts;

extern char *emsi_local_password;
extern char *emsi_remote_password;
extern char emsi_remote_comm[];

extern char *mkemsidat(int);
extern int scanemsidat(char*);

#endif

#ifndef TTYIO_H
#define TTYIO_H

#define PUTCHAR(x) tty_putc(x)
#define PUT(x,y) tty_put(x,y)
#define PUTSTR(x) tty_put(x,strlen(x))
#define GETCHAR(x) tty_getc(x)
#define UNGETCHAR(x) tty_ungetc(x)
#define GET(x,y,z) tty_get(x,y,z)
#define STATUS tty_status
#define TTYWAIT(x) ttywait(x)

#define STAT_SUCCESS 0
#define STAT_ERROR   1
#define STAT_TIMEOUT 2
#define STAT_EOFILE  4
#define STAT_HANGUP  8
#define STAT_EMPTY   16

#define SUCCESS (STATUS == 0)
#define ERROR (-STAT_ERROR)
#define TIMEOUT (-STAT_TIMEOUT)
#define HANGUP (-STAT_HANGUP)
#define EMPTY (-STAT_EMPTY)

#define NUL 0x00
#define SOH 0x01
#define STX 0x02
#define ETX 0x03
#define EOT 0x04
#define ENQ 0x05
#define ACK 0x06
#define BEL 0x07
#define BS  0x08
#define HT  0x09
#define LF  0x0a
#define VT  0x0b
#define FF  0x0c
#define CR  0x0d
#define SO  0x0e
#define SI  0x0f
#define DLE 0x10
#define DC1 0x11
#define DC2 0x12
#define DC3 0x13
#define DC4 0x14
#define NAK 0x15
#define SYN 0x16
#define ETB 0x17
#define CAN 0x18
#define EM  0x19
#define SUB 0x1a
#define ESC 0x1b
#define FS  0x1c
#define GS  0x1d
#define RS  0x1e
#define US  0x1f
#define TSYNC 0xae
#define YOOHOO 0xf1

extern int tty_status;

extern int tty_getc(int);
extern int tty_ungetc(int);
extern int tty_get(char*,int,int);
extern int tty_putc(int);
extern int tty_put(char*,int);
extern int ttywait(int);
extern void sendbrk(void);

#endif

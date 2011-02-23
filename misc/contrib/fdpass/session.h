/*
**  session.h
**
**  Structures for PASSWORD.FD
**
**  Copyright 1991-1993 Joaquim Homrighausen; All rights reserved.
**
**  Last revised: 93-06-21                         FrontDoor 2.11+
**
**  -------------------------------------------------------------------------
**  This information is not necessarily final and is subject to change at any
**  given time without further notice
**  -------------------------------------------------------------------------
*/
#ifndef __SESSION_H__
#define __SESSION_H__

#ifdef __cplusplus
extern "C" {
#endif

    /* Miscellaneous flags (STATUS) */

#define ACTIVE          0x01                /*Active, as opposed to inactive*/
#define DELETED         0x02                         /*Never written to disk*/
#define NOFREQS         0x04         /*Don't allow file requests from system*/
#define NOMAIL          0x08                  /*Don't allow mail from system*/
#define NOEMSI          0x10            /*No outbound EMSI sessions w/system*/
#define NOFTSC1         0x20          /*No outbound FTSC-1 sessions w/system*/
#define NOZAP           0x40          /*No outbound ZedZap sessions w/system*/
#define NOYOOHOO        0x80          /*No outbound YooHoo sessions w/system*/

/*
**  Note that any settings in the FDOPT environment variable applies to
**  all sessions (inbound AND outbound). For example, if FDOPT contains
**  "NOEMSI", NO EMSI handshaking will be done, regardless of settings
**  in the security manager
*/

typedef struct
    {
    unsigned short int  zone,                               /*System address*/
                        net,
                        node,
                        point;
    char                password[9];                        /*NUL terminated*/
    unsigned char       status;                                  /*See above*/
    }
    _PSW, *PSWPTR;

#ifdef __cplusplus
}
#endif
#endif

/* end of file "session.h" */

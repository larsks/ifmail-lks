/* usleep.c
 * simulate the usleep() function on machines where it isn't available
 * code taken from "mgetty+sendfax" (io.c) by Gert Doering
 */

#if !defined(USE_SELECT) && !defined(USE_POLL) && !defined(USE_NAP)
#define USE_SELECT
#endif

#ifdef USE_SELECT
#undef USE_POLL
#undef USE_NAP
#endif

#ifdef USE_POLL
#undef USE_NAP
#endif

#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#ifndef _NOSTDLIB_H
#include <stdlib.h>
#endif
#include <unistd.h>

#ifdef USE_POLL
# include <poll.h>
/*int poll _PROTO(( struct pollfd fds[], unsigned long nfds, int timeout ));*/
int poll ();
#endif

#ifdef USE_SELECT
# include <string.h>
# if defined (linux) || defined (sun) || defined (SVR4) || \
     defined (__hpux) || defined (MEIBE) || defined(sgi)
#  include <sys/types.h>
#  include <sys/time.h>
# else
#  include <sys/select.h>
# endif
#endif

void usleep ( unsigned long waittime )		/* wait waittime microseconds */
{
#ifdef USE_POLL
struct pollfd sdummy;
    poll( &sdummy, 0, waittime );
#else
#ifdef USE_NAP
    nap( waittime );
#else
#ifdef USE_SELECT
    struct timeval s;

    s.tv_sec  = waittime / 1000000;
    s.tv_usec = waittime % 1000000;
    select( 0, (fd_set *) NULL, (fd_set *) NULL, (fd_set *) NULL, &s );

#else				/* neither poll nor nap nor select available */
    if ( waittime < 2000000 ) waittime = 2000000;	/* round up, */
    sleep( waittime / 1000000);		/* a sleep of 1 may not sleep at all */
#endif	/* use select */
#endif	/* use nap */
#endif	/* use poll */
}

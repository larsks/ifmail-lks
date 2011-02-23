/*
 * signal() for BSD systems like SUNOS where a signal not automatically
 * interrupts a read() system call.  Contributed by Martin Junius.
*/

#include <signal.h>

void (*signal(sig, func))()
int sig;
void (*func)();
{
   struct sigaction sa, sa_old;

   sa.sa_mask    = 0;
   sa.sa_handler = func;
#if defined(SA_INTERRUPT) && defined(SA_RESETHAND)
   sa.sa_flags   = SA_INTERRUPT | SA_RESETHAND;
#else
   sa.sa_flags   = 0;
#endif

   sigaction(sig, &sa, &sa_old);

   return sa_old.sa_handler;
}

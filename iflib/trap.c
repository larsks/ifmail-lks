#ifdef NEED_TRAP

#include <stdio.h>
#include "lutil.h"
#include "trap.h"

#ifdef TESTING
#define logerr printf
#else
#include "lutil.h"
#endif

#if defined(linux)
#define __KERNEL__
#include <linux/signal.h>
#undef __KERNEL__
#define iBCS2
#elif defined(sun)
#define SUNSTYLE
#elif defined(SVR4)
#define SVR4STYLE
#endif

#include <signal.h>
#ifdef HAS_SYSLOG
#include <sys/types.h>
#include <unistd.h>
#include <syslog.h>
#endif

static char *nm="";

static struct _signames {
	int sig;
	char *name;
} signames[] =
{
#ifdef SIGSEGV
	{SIGSEGV,"SIGSEGV"},
#endif
#ifdef SIGBUS
	{SIGBUS,"SIGBUS"},
#endif
#ifdef SIGFPE
	{SIGFPE,"SIGFPE"},
#endif
#ifdef SIGSTKFLT
	{SIGSTKFLT,"SIGSTKFLT"},
#endif
	{0,NULL}
};

static char *signame(sig)
int sig;
{
	int i;
	static char buf[8];

	for (i=0;signames[i].name;i++)
		if (signames[i].sig == sig) break;
	if (signames[i].name) return signames[i].name;
	sprintf(buf,"%d",sig);
	return buf;
}

#if defined(iBCS2)
	static void trap(sig,context)
	int sig;
#ifdef PRE_21_LINUX
	struct sigcontext_struct context;
#else
	struct sigcontext context;
#endif
#elif defined(SVR4STYLE)
	static void trap(sig,context)
	int sig;
	siginfo_t *si;
#elif defined(SUNSTYLE)
	static void trap(sig,code,context,addr)
	int sig;
	int code;
	struct sigcontext *context;
	char *addr;
#else
	static void trap(sig)
	int sig;
#endif

{

#if defined(iBCS2)
	long *stack;
#ifdef TESTING
	int i;

	printf("conext.gs=%04hx,%04hx\n",context.__gsh,context.gs);
	printf("conext.fs=%04hx,%04hx\n",context.__fsh,context.fs);
	printf("conext.es=%04hx,%04hx\n",context.__esh,context.es);
	printf("conext.ds=%04hx,%04hx\n",context.__dsh,context.ds);
	printf("conext.edi=%08lx\n",context.edi);
	printf("conext.esi=%08lx\n",context.esi);
	printf("conext.ebp=%08lx\n",context.ebp);
	printf("conext.esp=%08lx\n",context.esp);
	printf("conext.ebx=%08lx\n",context.ebx);
	printf("conext.edx=%08lx\n",context.edx);
	printf("conext.ecx=%08lx\n",context.ecx);
	printf("conext.eax=%08lx\n",context.eax);
	printf("conext.trapno=%08lx\n",context.trapno);
	printf("conext.err=%08lx\n",context.err);
	printf("conext.eip=%08lx\n",context.eip);
	printf("conext.cs=%04hx,%04hx\n",context.__csh,context.cs);
	printf("conext.eflags=%08lx\n",context.eflags);
	printf("conext.esp_at_signal=%08lx\n",context.esp_at_signal);
	printf("conext.ss=%04hx,%04hx\n",context.__ssh,context.ss);
	printf("conext.i387=%08lx\n",context.i387);
	printf("conext.oldmask=%08lx\n",context.oldmask);
	printf("conext.cr2=%08lx\n",context.cr2);
	stack=(long *)context.esp_at_signal;
	for (i=-2;i<10;i++)
		printf("stack %2d: 0x%08lx: 0x%08lx\n",i,(long)&(stack[i]),stack[i]);
#endif

	signal(sig,SIG_DFL);  /* for BSD compatibility, won't harm SysV */

	logerr("caught %s at offset 0x%08lx",signame(sig),context.eip);

/*
	Follows an awful, nasty kludge.  The problem is that when
	interrupt occurs in a function compiled with
	-fomit-frame-pointer, there is no way (or at least I don't know
	one) to catch the beginning of the stack frame chain.  So, the
	only thing I could think of is to scan the stack in search of a
	value the _looks_like_ a frame chain pointer.  Generally, this
	approach may cause all kind of problems: from false backtrace
	information to segfault inside the handler.  If you know a
	better way, _please_ make me know!
*/

	for (stack=(long *)context.esp_at_signal;
	     stack && ((unsigned)((long *)stack[0]-stack) > 20);
	     stack++);
	for (;stack && stack[1];stack=(long*)stack[0])
	{
#ifdef TESTING
		printf("\n[0x%08lx] next 0x%08lx, ret 0x%08lx ",(long)stack,
			stack[0],stack[1]);
#endif
		logerr("called from 0x%08lx",stack[1]);
	}
#elif defined(SVR4STYLE)
	if (context) psiginfo(si,nm);
	else psignal(sig,nm);
#elif defined(SUNSTYLE)
	logerr("caught %s at offset 0x%08lx",signame(sig),addr);
#else
#warning Cannot provide detailed info on this architecture
	logerr("caught %s",signame(sig));
	psignal(sig,nm);
#endif

#ifdef HAS_SYSLOG
	debug(2,"closing syslog");
	closelog();
#endif
#ifdef TESTING
	printf("\n");
#else
	abort();	/* stupid, but BSD will cycle otherwise */
#endif
	return;
}

void catch(name)
char *name;
{
	int i=0;

	nm=name;

	for (i=0;signames[i].name;i++)
		debug(2,"catch: signal(%d) return %d",
			signames[i].sig,signal(signames[i].sig,trap));
	return;
}

#ifdef TESTING

void faulty(s)
int *s;
{
	int w=1234;

	memcpy(s,&w,sizeof(int));
	*s=1234;
	return;
}

int good(s)
int *s;
{
	faulty(s);
	return 0;
}

int main()
{
	int *s=NULL;
	int i;

	catch("trap test");
	printf("\nbefore fault\n");
	i=good(s);
	printf("\nafter fault\n");
	return 0;
}

#endif /* TESTING */

#endif /* NEED_TRAP */

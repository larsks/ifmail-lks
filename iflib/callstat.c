#include <sys/types.h>
#include <time.h>
#include <stdio.h>
#include "lutil.h"
#include "ftn.h"
#include "callstat.h"

extern char *stsname(faddr *,char);

static callstat st;

callstat *getstatus(addr)
faddr *addr;
{
	FILE *fp;
	time_t now;

	st.trytime=0L;
	st.tryno=0;
	st.trystat=0;

	if ((fp=fopen(stsname(addr,'f'),"r")))
	{
		fscanf(fp,"%lu %u %u",&st.trytime,&st.tryno,&st.trystat);
		fclose(fp);
	}

	(void)time(&now);

	return &st;
}

void putstatus(addr,incr,sts)
faddr *addr;
int incr;
int sts;
{
	FILE *fp;

	(void)getstatus(addr);
	if ((fp=fopen(stsname(addr,'f'),"w")))
	{
		if (sts == 0) st.tryno=0;
		else st.tryno+=incr;
		st.trystat=sts;
		(void)time(&st.trytime);
		fprintf(fp,"%lu %u %u\n",
			st.trytime,st.tryno,(unsigned)st.trystat);
		fclose(fp);
	}
	else
	{
		logerr("$cannot create status file for node %s",
			ascfnode(addr,0x1f));
	}
}

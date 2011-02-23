#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <time.h>
#include "lutil.h"
#include "config.h"

unsigned INT32 sequencer(void)
{
	FILE *fp;
	struct flock fl;
	unsigned long id;

	time((time_t*)&id);

	if ((fp=fopen(sequence,"r+")) == NULL)
		fp=fopen(sequence,"w");
	if (fp == NULL) return id;
	fl.l_type=F_WRLCK;
	fl.l_whence=0;
	fl.l_start=0L;
	fl.l_len=0L;
	fcntl(fileno(fp),F_SETLKW,&fl);
	fscanf(fp,"%lu",&id);
	rewind(fp);
	fprintf(fp,"%lu\n",++id);
	fclose(fp);

	return (unsigned INT32)id;
}

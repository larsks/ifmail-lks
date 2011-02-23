#ifndef CALLSTAT_H
#define CALLSTAT_H

#include <sys/types.h>
#include <time.h>
#include "ftn.h"

typedef struct _callstat {
	time_t trytime;
	int tryno;
	int trystat;
} callstat;

extern callstat *getstatus(faddr*);
extern void putstatus(faddr*,int,int);

#endif

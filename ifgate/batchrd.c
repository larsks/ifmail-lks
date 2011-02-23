#include <stdio.h>
#include <string.h>
#include "lutil.h"

static long counter = 0L;
static int batchmode = -1;
extern int usetmp;

/* Warning: this function modifies passed buffer in place */
static char *removecr(buf)
char *buf;
{
	char *p;

	if (!buf) return NULL;
	p=buf+strlen(buf)-2;
	if ((*p == '\r') && (*(p+1) == '\n')) {
		*p='\n';
		*(p+1)='\0';
	}
	return buf;
}

char *bgets(buf,count,fp)
char *buf;
int count;
FILE *fp;
{
	if (usetmp)
	{
		return removecr(fgets(buf,count,fp));
	}

	if ((batchmode == 1) && (counter > 0L) && (counter < (long)(count-1))) 
		count=(int)(counter+1L);
	if (fgets(buf,count,fp) == NULL) return NULL;
	switch (batchmode)
	{
	case -1: if (!strncmp(buf,"#! rnews ",9) || !strncmp(buf,"#!rnews ",8))
		{
			batchmode=1;
			sscanf(buf+8,"%ld",&counter);
			debug(18,"first chunk of input batch: %ld",counter);
			if (counter < (long)(count-1)) count=(int)(counter+1L);
			if (fgets(buf,count,fp) == NULL) return NULL;
			else
			{
				counter -= strlen(buf);
				return removecr(buf);
			}
		}
		else
		{
			batchmode=0;
			return removecr(buf);
		}

	case 0:	return removecr(buf);

	case 1:	if (counter <= 0L)
		{
			while (strncmp(buf,"#! rnews ",9) &&
		    		strncmp(buf,"#!rnews ",8))
			{
				loginf("batch out of sync: %s",buf);
				if (fgets(buf,count,fp) == NULL) return NULL;
			}
			sscanf(buf+8,"%ld",&counter);
			debug(18,"next chunk of input batch: %ld",counter);
			return NULL;
		}
		else
		{
			counter -= (long)strlen(buf);
			debug(19,"bread \"%s\", %ld left of this chunk",
				buf,counter);
			return removecr(buf);
		}
	}
	return removecr(buf);
}

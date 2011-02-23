#include <stdio.h>
#include <string.h>
#include "lutil.h"
#include "config.h"

char *unpacker(fn)
char *fn;
{
	FILE *fp;
	unsigned char buf[8],dbuf[80];
	int i;

	if ((fp=fopen(fn,"r")) == NULL) 
	{
		logerr("$Could not open file %s",S(fn));
		return NULL;
	}
	if (fread(buf,1,sizeof(buf),fp) != sizeof(buf))
	{
		logerr("$Could not read head of the file %s",S(fn));
		return NULL;
	}
	fclose(fp);
	dbuf[0]='\0';
	for (i=0;i<sizeof(buf);i++)
		if ((buf[i] >= ' ') && (buf[i] <= 127)) 
			sprintf((char*)dbuf+strlen(dbuf),"  %c",buf[i]);
		else
			sprintf((char*)dbuf+strlen(dbuf)," %02x",buf[i]);
	debug(2,"file head: %s",dbuf);

	if (memcmp(buf,"PK",2) == 0)         return unzip;
	if (*buf == 0x1a)                    return unarc;
	if (memcmp(buf+2,"-l",2) == 0)       return unlzh;
	if (memcmp(buf,"ZOO",3) == 0)        return unzoo;
	if (memcmp(buf,"`\352",2) == 0)      return unarj;
	if (memcmp(buf,"Rar",3) == 0)        return unrar;
	logerr("Unknown compress scheme in file %s",S(fn));
	return NULL;
}

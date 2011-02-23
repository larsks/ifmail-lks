/* 
A little program that places incoming letters to a queue.
It's only parameter is the full name of the directory where the queue
is to reside.
The program's executable must be suid and sgid to "fnet" or a similar
user in your system to avoid "Permission denied"s.
Your MTA's alias 'areafix' or 'filefix' or something else must point
to the program.
Example for sendmail:

areafix: "|/usr/lib/ifmail/queuemail  /var/spool/ifmail/aqueue"
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>

int main(int argc,char **argv)
{
	int c;
	FILE *f;
	char s1[1024],s2[1024],name[32];
	
	if (argc<2) return 2;
	sprintf(name,"%d%d",(int)getpid(),(int)time(NULL));
	sprintf(s1,"%s/%s",argv[1],"tmp");
	if (mkdir(s1,0700)==-1 && errno!=EEXIST) return 2;
	sprintf(s1,"%s/%s",s1,name);
	sprintf(s2,"%s/%s",argv[1],name);
	if ((f=fopen(s1,"w"))==NULL) return 2;
	while ((c=getchar())!=EOF) putc(c,f);	
	fclose(f);
	if (rename(s1,s2)==-1) return 2;
	return 0;
}


/*
	FIDO<->Internet Gate

	Vitaly Lev

	V1.1

*/

#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "fidonet.h"

/*
#define test
*/

extern	char *route(char *);

static char *version = {"@(#) fido.c 1.0.1 94/06/09 Lviv, (C)opyright by Alexander Trofimov, mod Vitaly Lev <Lion> 94/07/30"};
static char cmd1[1024];
static char fqueue[30];
int tc=0;

#define NCH	5
#define ADR_DONE '*'

main(int c, char **v)
{
FILE *p0;
char fname[15],c1[40];
int i,j;

#ifndef test	
	chdir(LOCKDIR);
	sprintf(c1,"ls %s",QDIR);
	p0 =  popen(c1,"r");


	while(fgets(fname,15,p0) != NULL)
		{
		fname[strlen(fname) - 1] = '\0';
#else
		strcpy(fname, "./xxx");
#endif
		send_mail(fname);

#ifndef test
		if (tc!=1)
			{
			chdir(MSGDIR);
			system(cmd1);
			chdir(LOCKDIR);
			}
		tc=0;
		}
	pclose(p0);
#endif
	exit(0);
}


send_mail(char *f)
{
char adr[255], via[255];
char str[200], *p, *p1;
FILE *f0;
long tpos,ppos;

	adr[0] = '\0';
	via[0] = '\0';

#ifndef test
	sprintf(fqueue,"%s/%s",QDIR,f);
	f0 = fopen(fqueue,"r+");
#else
	f0 = fopen(f,"r");
#endif

	while(fgets(str,200,f0) != NULL)
		{
		tpos = ftell(f0);
		if (strstr(str,"fidonet") == NULL) { ppos=tpos; continue; }
		fseek(f0,ppos+2,SEEK_SET); 
		if(getc(f0) == ADR_DONE)
			{
			tc=1;
			return(0);
			}

		fseek(f0,ppos+2,SEEK_SET);
		putc(ADR_DONE,f0);
		fseek(f0,tpos,SEEK_SET);
		ppos=tpos;

		if (strstr(str,"fidonet") == NULL) { ppos=tpos; continue; }
		ppos=tpos;

		if((p=strchr(str,':')) == NULL) continue;
		if((p1=strchr(str,',')) == NULL)
			{
			p++; *strchr(p,'"')= '\0';
			strcpy(adr,"\0");
			strcat(adr,p);
			
			p=via;
			p=route(adr);
			strcpy(via,p);

#ifndef test
			sprintf(cmd1,"/usr/local/lib/fnet/ifmail -r%s %s < %s\0",via,adr,f);
#else
			printf("/usr/local/lib/fnet/ifmail -r%s %s\n",via,adr);
#endif
		
			}
		else
			{
			p++; *strchr(p,'"')= '\0';
			p1+=2; *strchr(p1,':')='\0';
			strcat(via," "); strcat(via,p1);
			strcat(adr," "); strcat(adr,p);
			sprintf(cmd1,"/usr/local/lib/fnet/ifmail -r%s %s < %s\0",via,adr,f);
			}
		}	
}

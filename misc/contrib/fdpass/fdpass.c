/* Contributed by paiko!ricudis@forthnet.gr (Christos Ricudis) */

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include "session.h" /* session.h from fddev220.arj */


void main (int argc,char *argv[]) 
{
	int fd;
	_PSW koko;
	char addr[25];
	
	if (argc<2) {
		printf("Usage : fdpass <full pathname of PASSWORD.FD>\n");
		exit(1);
	}
	
	if (((fd=open(argv[1],O_RDONLY))==-1)) {
		printf("Cannot open %s\n",argv[1]);
		exit(2);
	}
	
	while (read(fd,&koko,sizeof(koko))!=0) {
		
		if (koko.point==0) 
			sprintf(addr,"%u:%u/%u",koko.zone,koko.net,koko.node);
		else
			sprintf(addr,"%u:%u/%u.%u",koko.zone,koko.net,koko.node,koko.point);
	
	
		if (!(koko.status & ACTIVE)) printf("#");

		if (strlen(koko.password)>0) 
			printf("password  %s  %s\n",addr,koko.password);
		
		if (koko.status & (NOFREQS|NOEMSI|NOZAP|NOYOOHOO)) {
			if (!(koko.status & ACTIVE)) printf("#");
			printf("options  (address %s)  ",addr);
			
			if (koko.status & NOFREQS)
				printf("nofreqs ");
			if (koko.status & NOEMSI) 
				printf("noemsi ");
			if (koko.status & NOZAP) 
				printf("nozap ");
			if (koko.status & NOYOOHOO)
				printf("nowazoo");
			
			printf("\n");
		}
	}
	close (fd);
}

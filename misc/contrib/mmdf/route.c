#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "ftn.h"
#include "version.h"

char my_addr[]="462/10";
char ROUTE_CFG[]="/etc/ifmail/route.cfg";
/* 
	global constants
*/
char adr[100][100];
char *a[100];
faddr *tfaddr=NULL;

/*
	routines
*/
char *routea(char *);

usage(){};

char *route(char *adr0)
{
FILE *f1;
char *a1;
int i=0;
char adr1[100];
char *adr2=adr1;
char *adrp;
char *routeadr;


	tfaddr=parsefaddr(adr0);
	adr2=ascfnode(tfaddr, 0x6);

	if (strcmp(adr2, my_addr) == 0)
		return(adr0);

	f1=fopen(ROUTE_CFG,"r");
	while(fgets(adr[i],100,f1) != NULL)
		{
		a[i]=adr[i];
		i++;
		}
	if(i==0) i++;
	a[i]=NULL;
/*
	i=0;
	while(a[i]!=NULL)
		printf("%s",a[i++]);
*/

	fclose(f1);

/*	printf("\n%s\n", adr2);*/

	if ((adrp=strchr(adr2, '.')) != NULL)
		*adrp='\0';

	if ((routeadr=routea(adr2)) == NULL)
		{
		adrp=(strchr(adr2, '/'));
		*(adrp+1)='*';
		*(adrp+2)='\0';
		if ((routeadr=routea(adr2)) == NULL)
			{
			strcpy(adr2, "*/*");
			if ((routeadr=routea(adr2)) == NULL)
				{
				printf("\nerror in route table\n");
				}
			}
		}

	if (routeadr != NULL)	
		{
		strcpy(adr1, routeadr);
		tfaddr=parsefnode(adr1);
		routeadr=adr1;
		routeadr=ascinode(tfaddr, 0x6);
		}

/*	printf("\nrouteaddr = %s\n", routeadr);*/

	return(routeadr);
}



char *routea(char *adr)
{
char *adrp1, *adrp2;
int i;

/*	printf("\n%s\n", adr);*/

	i=0;
	while (a[i] != NULL && (adrp1=strstr(a[i], adr)) == NULL)
		{
/*		printf("i=%i\n", adrp1);*/
		i++;
		}

	if (a[i] != NULL)
		{
		if (adrp1 != a[i])
			adrp1=a[i];

		if ((adrp2=strpbrk(adrp1, " \t\n")) != NULL)
			*adrp2='\0';
/*		printf("str=%s\n", adrp1);*/
		
		}
	else
		{
		adrp1=NULL;		
/*		printf("found NULL\n");*/
		}

	return(adrp1);
}

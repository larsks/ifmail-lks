#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include "ftn.h"
#include "lutil.h"
#include "config.h"

#ifndef PATH_MAX
#define PATH_MAX 512
#endif

/* increase this if you want to forward files from list/norm inbound */
#define NUMDIRS 2

extern FILE *openflo(faddr *,char);

void try_attach(fn,mode,addr,flavor)
char *fn;
int mode;
faddr *addr;
char flavor;
{
	FILE *flo;
	char pn[PATH_MAX],*p,*f;
	char *dirs[NUMDIRS];
	int i,j;

	debug(3,"Trying fileattach \"%s\" (mode %d) to %s (flavor %c)",
		S(fn),mode,ascfnode(addr,0x1f),flavor);

	if (fn == NULL) return;

	if (strstr(fn,"/../") ||
	    strstr(fn,"/..\\") ||
	    strstr(fn,"\\../") ||
	    strstr(fn,"\\..\\"))
	{
		loginf("attempt to attach file from otside restricted area \"%s\"",
			fn);
		return;
	}

	/*
	NOTE: you cannot attach a file from any point of the filesystem
	because that would be a security hole: anyone would be able to
	get any file from your system (readable for ifmail) sending a
	message to himself with an ATT flag.
	*/

	dirs[0]=pubdir;
	dirs[1]=protinbound;
	/* add more... */

	for (i=0;i<NUMDIRS;i++)
	for (j=0;j<2;j++)
	{
		if (j == 0)
		{
			strncpy(pn,dirs[i],sizeof(pn)-2);
			pn[strlen(dirs[i])]='/';
			for (f=fn;(*f) && (isspace(*f));f++);
			for (p=pn+strlen(dirs[i])+1;
				(*f) && (*f != '\n') && (!isspace(*f)) &&
				(p < pn+sizeof(pn)-1);
				p++,f++)
			{
				if (*f == '\\') *p='/';
				else *p=*f;
			}
			*p='\0';
		}
		else
		{
			for (p=pn+strlen(dirs[i])+1;*p;p++)
				*p=tolower(*p);
		}
	
		debug(3,"Checking \"%s\"",S(pn));

		if (access(pn,R_OK) == 0)
		{
			if ((flo=openflo(addr,flavor)) == NULL) return;

			if ((p=strrchr(pn,'/')))
			{
				f=fn;
				while (*(++p)) *(f++)=*p;
				*(f++)='\0';
			}

			debug(3,"attaching file \"%s\" to node %s",
				S(pn),ascfnode(addr,0x1f));

			fseek(flo,0L,SEEK_END);
			if (i > 0) /* only received files can be killed */
			switch (mode)
			{
			case 0:	break;
			case 1: fprintf(flo,"#"); break;
			case 2: fprintf(flo,"^"); break;
			}
			fprintf(flo,"%s\n",pn);

			fclose(flo);
			loginf("Attached \"%s\" to %s",
				fn,ascfnode(addr,0x1f));
			return;
		}
	}
	loginf("fileattach \"%s\" to %s failed: no file",
		S(fn),ascfnode(addr,0x1f));
	return;
}

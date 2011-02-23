/*
 * This routines have been written to replace the stuff of the Motorola
 * BSD Compatibility Package on a M88k with System V Release 4.1, which
 * did not seem to do the things described on the manual pages when I
 * tried to compile IFMail.
 * I don't know, if they are useful on other systems as well.
 *
 * Written by stefan@space.s-link.de (Stefan Westerfeld)
 */

#include "directory.h"
#include <stdio.h>

DIR *opendir(char *name)
{
	static struct e_dirent *de;

	if((de=(struct e_dirent *)malloc(sizeof(struct e_dirent))) == NULL) return(NULL);

	de->nbytes=0;
	if((de->fd=open(name,O_RDONLY)) > 0) return(de);

	free(de);
	return(NULL);
}

void closedir(struct e_dirent *de)
{
	close(de->fd);
	free(de);
}

void rewinddir(struct e_dirent *de)
{
	lseek(de->fd,0,SEEK_SET);
}

struct dirent *readdir(struct e_dirent *de)
{
	if(de->nbytes==0)
	{
		de->nbytes=getdents(de->fd,(struct dirent *)de->buf,1024);
		de->bp=de->buf;
		if(de->nbytes==0) return(NULL);
	}

	de->dp=(struct dirent *)de->bp;
	de->nbytes-=de->dp->d_reclen;
	de->bp+=de->dp->d_reclen;
	return(de->dp);
}

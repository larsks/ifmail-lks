/*
 * This routines have been written to replace the stuff of the Motorola
 * BSD Compatibility Package on a M88k with System V Release 4.1, which
 * did not seem to do the things described on the manual pages when I
 * tried to compile IFMail.
 * I don't know, if they are useful on other systems as well.
 *
 * Written by stefan@space.s-link.de (Stefan Westerfeld)
 */

#include <dirent.h>

#ifdef DONT_HAVE_DIRENT

#include <fcntl.h>

#define opendir my_opendir
#define readdir my_readdir
#define rewinddir my_rewinddir
#define closedir my_closedir


struct e_dirent
{
	struct dirent *dp;
	char buf[1024],*bp;
	int fd;
	long nbytes;
};

typedef struct e_dirent E_DIR;

#define DIR E_DIR

DIR *opendir(char *name);
void closedir(DIR *de);
void rewinddir(DIR *de);
struct dirent *readdir(DIR *de);

#endif

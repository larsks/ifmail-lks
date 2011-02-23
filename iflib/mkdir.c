#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>

#define MKDIR "/bin/mkdir"
#define DEVNULL "/dev/null"

int mkdir(dir)
char *dir;
{
	int pid,rc,status;

	if ((pid=fork()) == 0)
	{
		freopen(DEVNULL,"w",stdout);
		freopen(DEVNULL,"w",stderr);
		rc=execl(MKDIR,MKDIR,dir,NULL);
		return rc;
	}
	while (((rc=wait(&status)) == pid) || (rc == 0));
	return ((status & 0xff) == 0) ? (status >> 8) : (status & 0xff);
}

#include <sys/types.h>
#include <unistd.h>

int rename(old,new)
char *old,*new;
{
	register int rc;

	if ((rc=link(old,new)) == 0)
		return unlink(old);
	else return rc;
}

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

int main(argc,argv)
int argc;
char *argv[];
{
	struct stat st;
	time_t t=0L;
	int i=1;

	while (argv[i])
	{
		if (stat(argv[i],&st) == 0)
			if (st.st_mtime > t) t=st.st_mtime;
		i++;
	}
	if (t == 0L) time(&t);
	printf("%s",ctime(&t));
	return 0;
}

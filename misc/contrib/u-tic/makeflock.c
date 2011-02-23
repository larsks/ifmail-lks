#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>

void t2c(int);

void main(void)
{
	struct flock fl;
	char *tmp, *p[]={(char*)&fl.l_type,(char*)&fl.l_whence,
		(char*)&fl.l_start,(char*)&fl.l_len,(char*)&fl.l_pid};
	int i,j,min,n[]={0,1,2,3,4};

	for (i=0;i<4;i++)
	{
		for (j=i+1,min=i,tmp=p[i];j<5;j++)
			if (p[j]<tmp)
			{
				min=j;
				tmp=p[j];
			}
		tmp=p[i];
		p[i]=p[min];
		p[min]=tmp;
		j=n[i];	
		n[i]=n[min];
		n[min]=j;
	}

	for (i=0;i<5;i++)
		switch (n[i])
		{
		case 0:
			printf("s $_[1]\n");
			break;
		case 1:
			printf("s &SEEK_SET\n");
			break;
		case 2:
		case 3:
			t2c(sizeof(off_t));
			break;
		case 4:
			t2c(sizeof(pid_t));
			
		}
}

void t2c(int len)
{
	char ch, letters[]={'l','i','s','c'};
	int lengths[]={sizeof(long),sizeof(int),sizeof(short),sizeof(char)};
	int i;

	for (i=0;i<4;i++)
		if (len>=lengths[i]) break;
	len/=lengths[i];
	ch=letters[i];

	for (i=0;i<len;i++)
		printf("%c 0\n",ch);
}

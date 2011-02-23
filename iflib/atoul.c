#include <stdio.h>
#include <string.h>

unsigned long atoul(char*);
unsigned long atoul(str)
char *str;
{
	unsigned long x;

	if (sscanf(str,"%lu",&x) == 1)
		return x;
	else return 0xffffffff;
}

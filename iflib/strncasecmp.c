#include <ctype.h>

int
strncasecmp(s1,s2,n)
register unsigned char *s1, *s2;
register int n;
{
	while (*s1 && *s2 && (toupper(*s1) == toupper(*s2)) && --n)
	{
		s1++;
		s2++;
	}
	return (toupper(*s1) - toupper(*s2));
}

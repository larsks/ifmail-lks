#include <ctype.h>

int
strcasecmp(s1,s2)
register unsigned char *s1, *s2;
{
	while (*s1 && *s2 && (toupper(*s1) == toupper(*s2)))
	{
		s1++;
		s2++;
	}
	return (toupper(*s1) - toupper(*s2));
}

#include <stdio.h>
#include <string.h>
#include "bwrite.h"
#include "config.h"

/* write short (16bit) integer in "standart" byte order */
int iwrite(i,fp)
int i;
FILE *fp;
{
	putc(i & 0xff,fp);
	putc((i >> 8) & 0xff,fp);
	return 0;
}

/* write long (32bit) integer in "standart" byte order */
int lwrite(i,fp)
long i;
FILE *fp;
{
	int c;

	for (c=0;c<32;c+=8) putc((i >> c) & 0xff,fp);
	return 0;
}

int awrite(s,fp)
char *s;
FILE *fp;
{
	if (s) while (*s) putc(outtab[*(s++) & 0xff], fp);
	putc(0,fp);
	return 0;
}

/* write an arbitrary line to message body: change \n to \r,
   if a line starts with three dashes, insert a dash and a blank */
int cwrite(s,fp)
char *s;
FILE *fp;
{
	if ((strlen(s) >= 3) && (strncmp(s,"---",3) == 0) && (s[3] != '-')) {
		putc('-',fp);
		putc(' ',fp);
	}
	while (*s) 
	{
		if (*s == '\n') putc('\r',fp);
		else putc(outtab[*s & 0xff],fp);
		s++;
	}
	return 0;
}

/* write (multiline) header to kluge: change \n to ' ' and end line with \r */
int kwrite(s,fp)
char *s;
FILE *fp;
{
	while (*s) 
	{
		if (*(unsigned char*)s >= ' ')
			putc(outtab[*s & 0xff],fp);
		else switch (*s) {
		case '\r':
			if (*(s+1)) fprintf(fp,"\\r");
			break;
		case '\n':
			if (*(s+1)) fprintf(fp,"\\n");
			break;
		case '\\':
			fprintf(fp,"\\\\");
			break;
		case '\t':
			fprintf(fp,"\\t");
			break;
		case '\b':
			fprintf(fp,"\\b");
			break;
		default:
			fprintf(fp,"\\%03o",*(unsigned char*)s);
			break;
		}
		s++;
	}
	putc('\r',fp);
	return 0;
}

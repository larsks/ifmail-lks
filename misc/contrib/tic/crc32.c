/*
   crc32.c		Calculate CRC-32 on stdin, write result on stdout

   /home/repository/mine/tic/crc32.c,v 1.2 1994/04/21 18:50:56 cg Exp

*/
#include <stdio.h>

extern unsigned long crc_32_tab[];   /* crc table, defined below */
void makecrc();

void main()
{
	unsigned long crc;
	char c;
	long count;

	makecrc();
	crc = 0xFFFFFFFFL;
	count = 0;
	while (1)
	{
		c = getchar() & 0xff;
		if (feof(stdin))
			break;
		count++;
#if 0
		crc = crc_32_tab[((int)crc^(c)) & 0xff] ^ ((crc>>8) & 0x00ffffff);
#else
	    crc = crc_32_tab[((int)crc ^ (c)) & 0xff] ^ (crc >> 8);
#endif
	}
	printf("%lX\n", crc ^ 0xffffffffL);
}


/* ===========================================================================
 * Run a set of bytes through the crc shift register.  If s is a NULL
 * pointer, then initialize the crc shift register contents instead.
 * Return the current crc in either case.
 */
unsigned long updcrc(s, n)
    unsigned char *s;                 /* pointer to bytes to pump through */
    unsigned n;             /* number of bytes in s[] */
{
    register unsigned long c;         /* temporary variable */

    static unsigned long crc = (unsigned long)0xffffffffL; /* shift register contents */

    if (s == NULL) {
	c = 0xffffffffL;
    } else {
	c = crc;
	while (n--) {
	    c = crc_32_tab[((int)c ^ (*s++)) & 0xff] ^ (c >> 8);
	}
    }
    crc = c;
    return c ^ 0xffffffffL;       /* (instead of ~c for 64-bit machines) */
}

/*
 * Code to compute the CRC-32 table. Borrowed from 
 * gzip-1.0.3/makecrc.c.
 */

unsigned long crc_32_tab[256];

void
makecrc(void)
{
/* Not copyrighted 1990 Mark Adler	*/

  unsigned long c;      /* crc shift register */
  unsigned long e;      /* polynomial exclusive-or pattern */
  int i;                /* counter for all possible eight bit values */
  int k;                /* byte being shifted into crc apparatus */

  /* terms of polynomial defining this crc (except $x^{32}$): */
  static int p[] = {0,1,2,4,5,7,8,10,11,12,16,22,23,26};

  /* Make exclusive-or pattern from polynomial */
  e = 0;
  for (i = 0; i < sizeof(p)/sizeof(int); i++)
    e |= 1L << (31 - p[i]);

  crc_32_tab[0] = 0;

  for (i = 1; i < 256; i++)
  {
    c = 0;
    for (k = i | 256; k != 1; k >>= 1)
    {
      c = c & 1 ? (c >> 1) ^ e : c >> 1;
      if (k & 1)
        c ^= e;
    }
    crc_32_tab[i] = c;
  }
}


#include <stdio.h>
#include "hash.h"
#include "lhash.h"

void hash_update_s(id,mod)
unsigned long *id;
char *mod;
{
	*id ^= lh_strhash(mod);
}

void hash_update_n(id,mod)
unsigned long *id;
unsigned long mod;
{
	char buf[32];
	sprintf(buf,"%030lu",mod);
	*id ^= lh_strhash(buf);
}

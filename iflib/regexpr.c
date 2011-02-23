#include <stdlib.h>
#if defined(HAS_REGEX_H)
#include <regex.h>
#elif defined(HAS_LIBGEN_H)
#include <libgen.h>
#else
char *regcmp();
char *regex();
#endif

static char *compiled_reg=NULL;
extern char *__loc1;

char *re_comp(mask)
char *mask;
{
	if (compiled_reg) free(compiled_reg);
	compiled_reg=regcmp(mask,NULL);
	return NULL;
}

int re_exec(str)
char *str;
{
	char *first_unmatched;

	return ((first_unmatched=regex(compiled_reg,str)) &&
		(__loc1 == str) &&
		(*first_unmatched == '\0'));
}

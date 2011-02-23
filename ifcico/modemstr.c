#include <stdio.h>
#include "config.h"
#include "nodelist.h"

static modem_string *tmp=NULL;

char *get_modem_string(ms,nlent)
modem_string *ms;
node *nlent;
{
	while (ms && ((ms->expr) && !flagexp(ms->expr,nlent))) 
		ms=ms->next;
	if (ms)
	{
		tmp=ms->next;
		return ms->line;
	}
	else
	{
		tmp=NULL;
		return NULL;
	}
}

char *next_modem_string(nlent)
node *nlent;
{
	return get_modem_string(tmp,nlent);
}

#include <stdlib.h>
#include <string.h>
#include "session.h"
#include "xutil.h"
#include "lutil.h"
#include "nodelist.h"
#include "config.h"

extern char *get_modem_string(modem_string*,node*);
extern char *next_modem_string(node*);

int localoptions;

static struct _ktab {
	char *key;
	int flag;
} ktab[] = {
	{"Call",NOCALL},
	{"Hold",NOHOLD},
	{"PUA",NOPUA},
	{"WaZOO",NOWAZOO},
	{"EMSI",NOEMSI},
	{"Freqs",NOFREQS},
	{"Zmodem",NOZMODEM},
	{"ZedZap",NOZEDZAP},
	{"Janus",NOJANUS},
	{"Hydra",NOHYDRA},
	{"Tcp",NOTCP},
	{NULL,0}
};

void rdoptions(node*);
void rdoptions(nlent)
node *nlent;
{
	char *str,*s,*p;
	int i;
	int match;

	localoptions=0;

	for (str=get_modem_string(options,nlent);str;
		str=next_modem_string(nlent))
	{
		s=xstrcpy(str);
		debug(10,"applicable option string: \"%s\"",S(s));
		for (p=strtok(s," \t,");p;p=strtok(NULL," \t,"))
		{
			match=0;
			for (i=0;ktab[i].key;i++)
			{
				if (strcasecmp(ktab[i].key,p) == 0)
				{
					localoptions &= ~(ktab[i].flag);
					match=1;
				}
				else if ((strncasecmp("No",p,2) == 0) &&
				    (strcasecmp(ktab[i].key,p+2) == 0))
				{
					localoptions |= ktab[i].flag;
					match=1;
				}
			}
			if (!match) logerr("illegal option \"%s\" ignored",p);
		}
		free(s);
		debug(10,"new options value: 0x%04x",localoptions);
	}
	s=NULL;
	for (i=0;ktab[i].key;i++)
		{
			s=xstrcat(s," ");
			if (localoptions & ktab[i].flag)
				s=xstrcat(s,"No");
			s=xstrcat(s,ktab[i].key);
		}
	loginf("options:%s",s);
	free(s);
}

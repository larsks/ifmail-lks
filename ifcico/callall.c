#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "xutil.h"
#include "lutil.h"
#include "ftn.h"
#include "nodelist.h"
#include "config.h"
#include "scanout.h"

static int each(faddr*,char,int,char*);

static fa_list *alist=NULL;

fa_list *callall(void)
{
	fa_list *tmp;
	int rc;

	debug(7,"callall requested");

	if (alist) 
	{
		for (tmp=alist;tmp;tmp=alist)
		{
			alist=tmp->next;
			tidy_faddr(tmp->addr);
			free(tmp);
		}
		alist=NULL;
	}

	if ((rc=scanout(each)))
	{
		logerr("Error scanning outbound, aborting");
		return NULL;
	}

	return alist;
}

static int each(addr,flavor,isflo,fname)
faddr *addr;
char flavor;
int isflo;
char *fname;
{
	fa_list **tmp;

	if ((flavor == 'h') ||
	    ((isflo != OUT_PKT) && (isflo != OUT_FLO)))
		return 0;

	for (tmp=&alist;*tmp;tmp=&((*tmp)->next))
		if (((*tmp)->addr->zone == addr->zone) &&
		    ((*tmp)->addr->net == addr->net) &&
		    ((*tmp)->addr->node == addr->node) &&
		    ((*tmp)->addr->point == addr->point) &&
		    (((*tmp)->addr->domain == NULL) ||
		     (addr->domain == NULL) ||
		     (strcasecmp((*tmp)->addr->domain,addr->domain) == 0)))
			break;
	if (*tmp == NULL)
	{
		*tmp=(fa_list *)xmalloc(sizeof(fa_list));
		(*tmp)->next=NULL;
		(*tmp)->addr=(faddr *)xmalloc(sizeof(faddr));
		(*tmp)->addr->name=NULL;
		(*tmp)->addr->zone=addr->zone;
		(*tmp)->addr->net=addr->net;
		(*tmp)->addr->node=addr->node;
		(*tmp)->addr->point=addr->point;
		(*tmp)->addr->domain=xstrcpy(addr->domain);

		debug(7,"has mail to %s",ascfnode((*tmp)->addr,0x1f));
	}
	return 0;
}

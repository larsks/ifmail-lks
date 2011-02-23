%token NUMBER PHSTR TIMESTR ADDRSTR IDENT SPEED PHONE TIME ADDRESS DOW ANY WK WE SUN MON TUE WED THU FRI SAT EQ NE GT GE LT LE LB RB AND OR NOT XOR COMMA ASTERISK AROP LOGOP

%{
#include <sys/types.h>
#include <stdlib.h>
#include <time.h>
#include "xutil.h"
#include "lutil.h"
#include "nodelist.h"

node *nodebuf;
int result;
struct tm *now;
%}

%%
fullline	: expression
			{debug(13,"fulline: return %d",$1);result=$1;}
		;
expression	: elemexp
			{debug(13,"elem.expr %d",$1);$$ = $1;}
		| NOT expression
			{debug(13,"not.expr %d",$2);$$ = !($2);}
		| expression LOGOP expression
			{debug(13,"log.expr %d %d %d",$1,$2,$3);$$ = logic($1,$2,$3);}
		| LB expression RB
			{debug(13,"backeted.expr %d",$2);$$ = $2;}
		;
elemexp		: flag
			{debug(13,"flag %d",$1);$$ = match($1);}
		| SPEED AROP NUMBER
			{debug(13,"speed on num %d %d",$2,$3);$$ = checkspeed($2,$3);}
		| PHONE PHSTR
			{debug(13,"phone num %d",$2);$$ = checkphone();}
		| PHONE NUMBER
			{debug(13,"phone num %d",$2);$$ = checkphone();}
		| TIME timestring
			{debug(13,"time %d",$2);$$ = $2;}
		| ADDRESS ADDRSTR
			{debug(13,"address %d",$2);$$ = $2;}
		;
flag		: IDENT
			{debug(13,"ident %d",$1);$$ = $1;}
		;
timestring	: TIMESTR
			{debug(13,"timelem %d",$1);$$ = $1;}
		| TIMESTR COMMA timestring
			{debug(13,"log.expr %d %d %d",$1,OR,$3);$$ = logic($1,OR,$3);}
%%

#include "flaglex.c"

int match(fl)
int fl;
{
	int i;

	debug(13,"match: %d",fl);
	if (fl == -1)
	{
		for (i=0;(i<MAXUFLAGS) && (nodebuf->uflags[i]);i++)
			if (strcasecmp(yytext,nodebuf->uflags[i]) == 0) 
				return 1;
		return 0;
	}
	else
	{
		return ((nodebuf->flags & fl) != 0);
	}
}

int logic(e1,op,e2)
int e1,op,e2;
{
	debug(13,"logic: %d %d %d",e1,op,e2);
	switch (op)
	{
	case AND:	return(e1 && e2);
	case OR:	return(e1 || e2);
	case XOR:	return(e1 ^ e2);
	default:	logerr("Parser: internal error: invalid logical operator");
			return 0;
	}
}

int checkspeed(op,speed)
int op,speed;
{
	debug(13,"checkspeed: %d %d",op,speed);
	switch (op)
	{
	case EQ:	return(nodebuf->speed == speed);
	case NE:	return(nodebuf->speed != speed);
	case GT:	return(nodebuf->speed >  speed);
	case GE:	return(nodebuf->speed >= speed);
	case LT:	return(nodebuf->speed <  speed);
	case LE:	return(nodebuf->speed <= speed);
	default:	logerr("Parser: internal error: invalid arithmetic operator");
			return 0;
	}
}

int checkphone(void)
{
	debug(13,"checkphone: \"%s\"",yytext);
	if (nodebuf->phone == NULL) return 0;
	if (strncasecmp(yytext,nodebuf->phone,strlen(yytext)) == 0) return 1;
	else return 0;
}

int flagexp(expr,nl)
char *expr;
node *nl;
{
	time_t tt;
	char *p;

	debug(13,"check expression \"%s\"",expr);
	nodebuf=nl;
	(void)time(&tt);
	now=localtime(&tt);
	p=xstrcpy(expr);
	yyPTR=p;
#ifdef FLEX_SCANNER  /* flex requires reinitialization */
	yy_init=1;
#endif
	result=0;
	if ((yyparse()))
	{
		logerr("could not parse expression \"%s\", assume `false'",expr);
		free(p);
		return 0;
	}
	debug(13,"checking result is \"%s\"",result?"true":"false");
	free(p);
	return result;
}

int yyerror(s)
char *s;
{
	logerr("parser error: %s",s);
	return 0;
}

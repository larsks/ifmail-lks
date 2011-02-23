%{
#include <stdlib.h>
#include <string.h>
#include "ftn.h"
#include "lutil.h"
#include "nodelist.h"

static char *yyPTR = NULL;
static int yyBUFL;

#ifndef yywrap
#define yywrap() 1
#endif

#ifdef FLEX_SCANNER

#undef YY_INPUT
#define YY_INPUT(buf,result,max_size) \
	if ((yyPTR == NULL) || (*yyPTR == '\0')) \
	{ \
		debug(13,"YY_INPUT unavail, %d reqd",max_size); \
		result=YY_NULL; \
	} \
	else \
	{ \
		yyBUFL=strlen(yyPTR); \
		debug(13,"YY_INPUT \"%s\" (%d) avail, %d reqd",\
			yyPTR,yyBUFL,max_size); \
		if (yyBUFL > max_size) yyBUFL=max_size; \
		memcpy(buf,yyPTR,yyBUFL); \
		yyPTR += yyBUFL; \
		result=yyBUFL; \
	}

#else /* this is most probably native lex? */

#undef input
#undef output
#undef unput

#define input() (((yyPTR == NULL) || (*yyPTR == '\0'))?NULL:*yyPTR++)
#define output(c)
#define unput(c) (*--yyPTR = (c))

#endif

int flagfor(char *);
int chkaddr(char*);
int chktime(char*);
int yylval;
%}

Blank		[ \t\n]+
Speed		[Ss][Pp][Ee][Ee][Dd]
Phone		[Pp][Hh][Oo][Nn][Ee]
Time		[Tt][Ii][Mm][Ee]
Address		[Aa][Dd][Dd][Rr][Ee][Ss][Ss]
Sun		[Ss][Uu][Nn]
Mon		[Mm][Oo][Nn]
Tue		[Tt][Uu][Ee]
Wed		[Ww][Ee][Dd]
Thu		[Tt][Hh][Uu]
Fri		[Ff][Rr][Ii]
Sat		[Ss][Aa][Tt]
Any		[Aa][Nn][Yy]
Wk		[Ww][Kk]
We		[Ww][Ee]
Eq		==
Ne		!=
Gt		\>
Ge		\>=
Lt		\<
Le		\<=
And		\&
Or		\|
Not		\!
Xor		[Xx][Oo][Rr]
Lb		\(
Rb		\)
Comma		\,
Asterisk	\*
Dow		({Sun}|{Mon}|{Tue}|{Wed}|{Thu}|{Fri}|{Sat}|{Any}|{Wk}|{We})
Hour		(([0-1][0-9])|(2[0-3]))
Minute		[0-5][0-9]
Decimal		[1-9][0-9]*
Numeric		[0-9][0-9]*
Phstr		[0-9-]*-[0-9-]*
Ident		[A-Za-z][A-Za-z0-9]*
Timestr		{Dow}({Hour}{Minute}\-{Hour}{Minute})?
Addrstr		(({Decimal}|\*):)?({Decimal}|\*)\/({Numeric}|\*)(.({Numeric}|\*))?(@({Ident}|\*))?
%%
{Blank}		;
{Decimal}	{debug(13,"Decimal: \"%s\"",yytext);yylval=strtol(yytext,NULL,0);return(NUMBER);}
{Timestr}	{debug(13,"Timestr: \"%s\"",yytext);yylval=chktime(yytext);return(TIMESTR);}
{Phstr}		{debug(13,"Phstr: \"%s\"",yytext);yylval=PHSTR;return(PHSTR);}
{Eq}		{yylval=EQ;return(AROP);}
{Ne}		{yylval=NE;return(AROP);}
{Gt}		{yylval=GT;return(AROP);}
{Ge}		{yylval=GE;return(AROP);}
{Lt}		{yylval=LT;return(AROP);}
{Le}		{yylval=LE;return(AROP);}
{And}		{yylval=AND;return(LOGOP);}
{Or}		{yylval=OR;return(LOGOP);}
{Not}		{yylval=NOT;return(NOT);}
{Xor}		{yylval=XOR;return(LOGOP);}
{Lb}		{yylval=LB;return(LB);}
{Rb}		{yylval=RB;return(RB);}
{Comma}		{yylval=COMMA;return(COMMA);}
{Asterisk}	{yylval=ASTERISK;return(ASTERISK);}
{Speed}		{yylval=SPEED;return(SPEED);}
{Phone}		{yylval=PHONE;return(PHONE);}
{Time}		{yylval=TIME;return(TIME);}
{Address}	{yylval=ADDRESS;return(ADDRESS);}
{Ident}		{debug(13,"Ident: \"%s\"",yytext);yylval=flagfor(yytext);return(IDENT);}
{Addrstr}	{debug(13,"Addrstr: \"%s\"",yytext);yylval=chkaddr(yytext);return(ADDRSTR);}
%%

int flagfor(str)
char *str;
{
	int i;

	debug(13,"flagfor \"%s\"",str);
	for (i=0;fkey[i].key;i++)
		if (strcasecmp(str,fkey[i].key) == 0) return fkey[i].flag;
	return -1;
}

int chkaddr(str)
char *str;
{
	faddr *addr;
	int rc=1;

	if ((addr=parsefnode(str)) == NULL)
	{
		logerr("unparsable address \"%s\" in expression",str);
		return 0;
	}
	debug(13,"chkaddr: does spec %s match remote address ?",
		ascfnode(addr,0x1f));
	if ((addr->domain) && (nodebuf->addr.domain) && 
	    (strcasecmp(addr->domain,nodebuf->addr.domain) != 0))
		rc=0;
	if (((int)addr->zone != -1)  &&(addr->zone != 0) &&
		(addr->zone != nodebuf->addr.zone)) rc=0;
	if (((int)addr->net != -1) && (addr->net != nodebuf->addr.net)) rc=0;
	if (((int)addr->node != -1) && (addr->node != nodebuf->addr.node)) rc=0;
	if (((int)addr->point != -1) && (addr->point != nodebuf->addr.point)) rc=0;
	tidy_faddr(addr);
	return rc;
}

int chktime(str)
char *str;
{
	int h1,h2,m1,m2,beg,end,cur,dayok,day;

	if (strncasecmp(str,"Sun",3) == 0) day=0;
	else if (strncasecmp(str,"Mon",3) == 0) day=1;
	else if (strncasecmp(str,"Tue",3) == 0) day=2;
	else if (strncasecmp(str,"Wed",3) == 0) day=3;
	else if (strncasecmp(str,"Thu",3) == 0) day=4;
	else if (strncasecmp(str,"Fri",3) == 0) day=5;
	else if (strncasecmp(str,"Sat",3) == 0) day=6;
	else if (strncasecmp(str,"Any",3) == 0) day=-1;
	else if (strncasecmp(str,"Wk",2) == 0) day=-2;
	else if (strncasecmp(str,"We",2) == 0) day=-3;
	else day=-4;

	debug(13,"chkday: does day %d match spec %d ?",now->tm_wday,day);
	if (day >= 0) dayok=(now->tm_wday == day);
	else switch (day)
	{
	case -3: dayok=((now->tm_wday == 0) || (now->tm_wday == 6)); break;
	case -2: dayok=((now->tm_wday != 0) && (now->tm_wday != 6)); break;
	case -1: dayok=1; break;
	default: logerr("internal error: chkday got %d",day); dayok=0; break;
	}

	if (dayok == 0) return 0;

	while (*str && ((*str < '0') || (*str > '9'))) str++;
	if (*str == '\0')
	{
		return 1;
	}
	if (sscanf(str,"%02d%02d-%02d%02d",&h1,&m1,&h2,&m2) != 4)
	{
		logerr("invalid time string \"%s\" in expression",str);
		return 0;
	}
	debug(13,"chktime: is %02d:%02d between %02d:%02d and %02d:%02d ?",
		now->tm_hour,now->tm_min,h1,m1,h2,m2);
	cur=now->tm_hour*60+now->tm_min;
	beg=h1*60+m1;
	end=h2*60+m2;
	if (end > beg)
	{
		return ((cur >= beg) && (cur <= end));
	}
	else
	{
		return ((cur >= beg) || (cur <= end));
	}
}

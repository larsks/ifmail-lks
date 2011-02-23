#ifndef LUTIL_H
#define LUTIL_H

#define S(x) (x)?(x):"(null)"

extern unsigned long verbose;
extern char *myname;
extern int logfacility;
void setmyname(char*);
unsigned long setverbose(char*);
void debug(unsigned long,char*,...);
void loginf(char*,...);
void logerr(char*,...);
char *date(long);
char *printable(char*,int);
char *printablec(char);
#endif

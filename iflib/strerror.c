char *strerror(errno)
int errno;
{
#ifndef HAS_SYS_ERRLIST
	extern char *sys_errlist[];
#endif
	extern int sys_nerr;
	if ((errno >= 0) && (errno <= sys_nerr))
		return sys_errlist[errno];
	else
		return "unknown error";
}

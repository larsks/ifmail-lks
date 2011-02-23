#include <sys/types.h>
#include <time.h>

time_t mtime2sl(time_t);
time_t sl2mtime(time_t);
time_t mtime2tl(time_t);
time_t tl2mtime(time_t);

time_t gmtoff(time_t);
time_t gmtoff(tt)
time_t tt;
{
	struct tm lt;
#ifdef DONT_HAVE_TM_GMTOFF
	struct tm gt;
	time_t offset;

	lt=*localtime(&tt);
	gt=*gmtime(&tt);
	offset=gt.tm_yday-lt.tm_yday;
	if (offset > 1) offset=-24;
	else if (offset < -1) offset=24;
	else offset*=24;
	offset+=gt.tm_hour-lt.tm_hour;
	offset*=60;
	offset+=gt.tm_min-lt.tm_min;
	offset*=60;
	offset+=gt.tm_sec-lt.tm_sec;
	return offset;
#else
	lt=*localtime(&tt);
	return -lt.tm_gmtoff;
#endif
}

time_t mtime2sl(tt)
time_t tt;
{
	return tt-gmtoff(tt);
}
time_t sl2mtime(tt)
time_t tt;
{
	return tt+gmtoff(tt);
}
time_t mtime2tl(tt)
time_t tt;
{
	time_t tlt=0L;
	struct tm *tm;

	tm=localtime(&tt);
	tlt |= ((tm->tm_year)-1980)<<25;
	tlt |= (tm->tm_mon)<<21;
	tlt |= (tm->tm_mday)<<16;
	tlt |= (tm->tm_hour)<<11;
	tlt |= (tm->tm_min)<<5;
	tlt |= (tm->tm_sec)>>1;
	return tlt;
}
time_t tl2mtime(tt)
time_t tt;
{
	struct tm tm;

	tm.tm_year= ((tt>>25)&0x7f)+1980;
	tm.tm_mon=  (tt>>21)&0x0f;
	tm.tm_mday=  (tt>>16)&0x1f;
	tm.tm_hour= (tt>>11)&0x1f;
	tm.tm_min=  (tt>>5 )&0x3f;
	tm.tm_sec=  ((tt    )&0x1f)*2;
	return mktime(&tm);
}

/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/file.h>
#include <FFOS/time.h>

int fftime_cmp(const fftime *t1, const fftime *t2)
{
	if (t1->s == t2->s) {
		if (t1->mcs == t2->mcs)
			return 0;
		if (t1->mcs < t2->mcs)
			return -1;
	}
	else if (t1->s < t2->s)
		return -1;
	return 1;
}

void fftime_totm(struct tm *tt, const ffdtm *dt)
{
	FF_ASSERT(dt->year >= 1970);
	tt->tm_year = dt->year - 1900;
	tt->tm_mon = dt->month - 1;
	tt->tm_mday = dt->day;
	tt->tm_hour = dt->hour;
	tt->tm_min = dt->min;
	tt->tm_sec = dt->sec;
}

void fftime_fromtm(ffdtm *dt, const struct tm *tt)
{
	dt->year = tt->tm_year + 1900;
	dt->month = tt->tm_mon + 1;
	dt->day = tt->tm_mday;
	dt->weekday = tt->tm_wday;
	dt->hour = tt->tm_hour;
	dt->min = tt->tm_min;
	dt->sec = tt->tm_sec;
	dt->msec = 0;
}

void fftime_addms( fftime *t, uint64 ms )
{
	t->mcs += (ms % 1000) * 1000;
	t->s += (uint)(ms / 1000);
	if (t->mcs >= 1000000) {
		t->mcs -= 1000000;
		t->s++;
	}
}

ffbool fftime_ok(const ffdtm *t)
{
	return !(t->year > 4095
		|| t->month > 12
		|| t->day > 31
		|| t->weekday > 6
		|| t->hour > 23
		|| t->min > 59
		|| t->sec > 59
		|| t->msec > 999);
}

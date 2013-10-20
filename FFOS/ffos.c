/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/file.h>
#include <FFOS/time.h>
#include <FFOS/socket.h>

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

void fftime_diff(const fftime *start, fftime *stop)
{
	if (stop->mcs >= start->mcs) {
		stop->s -= start->s;
		stop->mcs -= start->mcs;
	}
	else {
		stop->s -= start->s + 1;
		stop->mcs += 1000000 - start->mcs;
	}
}

#if defined FF_WIN || (defined FF_LINUX && defined FF_OLDLIBC)
ffskt ffskt_accept(ffskt listenSk, struct sockaddr *a, socklen_t *addrSize, int flags)
{
	ffskt sk = accept(listenSk, a, addrSize);
	if ((flags & SOCK_NONBLOCK) && 0 != ffskt_nblock(sk, 1)) {
		ffskt_close(sk);
		return FF_BADSKT;
	}
	return sk;
}
#endif

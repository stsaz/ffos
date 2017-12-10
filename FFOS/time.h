/**
Date & time.
Copyright (c) 2013 Simon Zolin
*/

#pragma once

#include <FFOS/types.h>

#include <time.h>

typedef struct fftime {
	uint s;
	uint mcs;
} fftime;

static FFINL void fftime_null(fftime *t) {
	t->s = t->mcs = 0;
}

/** Get UTC time. */
FF_EXTN void fftime_now(fftime *t);

/** Convert 'time_t' to 'fftime'. */
static FFINL void fftime_fromtime_t(fftime *t, time_t ts) {
	t->s = (int)ts;
	t->mcs = 0;
}

#ifdef FF_UNIX
/** Convert 'timespec' to 'fftime'. */
static FFINL void fftime_fromtimespec(fftime *t, const struct timespec *ts) {
	t->s = ts->tv_sec;
	t->mcs = ts->tv_nsec / 1000;
}
#endif

/** Set time as nanoseconds value. */
static FFINL void fftime_setns(fftime *t, uint64 ns) {
	t->s = (uint)(ns / 1000000000);
	t->mcs = (uint)((ns % 1000000000) / 1000);
}

/** Set time as microseconds value. */
static FFINL void fftime_setmcs(fftime *t, uint64 mcsecs) {
	t->s = (uint)(mcsecs / 1000000);
	t->mcs = (uint)(mcsecs % 1000000);
}

/** Add milliseconds. */
FF_EXTN void fftime_addms(fftime *t, uint64 ms);

FF_EXTN void fftime_add(fftime *t, const fftime *t2);

/** Get time value in microseconds. */
static FFINL uint64 fftime_mcs(const fftime *t) {
	return (uint64)t->s * 1000000 + t->mcs;
}

/** Get time value in milliseconds. */
static FFINL uint64 fftime_ms(const fftime *t) {
	return (uint64)t->s * 1000 + t->mcs / 1000;
}


/** Convert between Windows FILETIME structure & fftime. */
typedef struct fftime_winftime {
	uint lo, hi;
} fftime_winftime;

enum { FFTIME_100NS = 116444736000000000ULL };

static FFINL fftime fftime_from_winftime(const fftime_winftime *ft)
{
	fftime t = { 0, 0 };
	uint64 i = ((uint64)ft->hi << 32) | ft->lo;
	if (i > FFTIME_100NS)
		fftime_setmcs(&t, (i - FFTIME_100NS) / 10);
	return t;
}

static FFINL void fftime_to_winftime(const fftime *t, fftime_winftime *ft)
{
	uint64 d = fftime_mcs(t) * 10 + FFTIME_100NS;
	ft->lo = (uint)d;
	ft->hi = (uint)(d >> 32);
}

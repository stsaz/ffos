/**
Date & time.
Copyright (c) 2013 Simon Zolin
*/

#pragma once

#include <FFOS/types.h>

#include <time.h>

typedef struct fftime {
	/**
	2 possible applications for Gregorian calendar:
	 . Absolute: seconds before or after Jan 1, 1 AD
	 . UNIX-timestamp: seconds after Jan 1, 1970.
	Functions fftime_now(), fffile_settime(), etc. use UNIX-timestamp format. */
	int64 sec;
	uint nsec;
} fftime;

#define fftime_empty(t)  ((t)->sec == 0 && (t)->nsec == 0)

#define fftime_sec(t)  ((t)->sec)
#define fftime_msec(t)  ((t)->nsec / 1000000)
#define fftime_usec(t)  ((t)->nsec / 1000)
#define fftime_nsec(t)  ((t)->nsec)

#define fftime_setsec(t, s)  ((t)->sec = (s))
#define fftime_setmsec(t, ms)  ((t)->nsec = (ms) * 1000000)
#define fftime_setusec(t, us)  ((t)->nsec = (us) * 1000)
#define fftime_setnsec(t, ns)  ((t)->nsec = (ns))

static FFINL void fftime_null(fftime *t) {
	t->sec = 0,  t->nsec = 0;
}

/** Windows: prepare higher precision timer for fftime_now(). */
FF_EXTN void fftime_init(void);

/** Get UTC time. */
FF_EXTN void fftime_now(fftime *t);


typedef struct fftime_zone {
	int off; //UTC+X (in seconds)
	uint have_dst; //may have daylight saving time rules
} fftime_zone;

/** Get local timezone. */
FF_EXTN void fftime_local(fftime_zone *tz);

/** Store local timezone for fast timestamp conversion using FFTIME_TZLOCAL. */
FF_EXTN void fftime_storelocal(const fftime_zone *tz);


/** Convert 'time_t' to 'fftime'. */
static FFINL void fftime_fromtime_t(fftime *t, time_t ts) {
	t->sec = ts;
	t->nsec = 0;
}

static FFINL time_t fftime_to_time_t(const fftime *t)
{
	return t->sec;
}

#ifdef FF_UNIX
/** Convert 'timeval' to 'fftime'. */
static FFINL void fftime_fromtimeval(fftime *t, const struct timeval *ts) {
	fftime_setsec(t, ts->tv_sec);
	fftime_setusec(t, ts->tv_usec);
}

static FFINL void fftime_to_timeval(const fftime *t, struct timeval *ts)
{
	ts->tv_sec = fftime_sec(t);
	ts->tv_usec = fftime_usec(t);
}

/** Convert 'timespec' to 'fftime'. */
static FFINL void fftime_fromtimespec(fftime *t, const struct timespec *ts) {
	fftime_setsec(t, ts->tv_sec);
	fftime_setnsec(t, ts->tv_nsec);
}

static FFINL void fftime_to_timespec(const fftime *t, struct timespec *ts)
{
	ts->tv_sec = fftime_sec(t);
	ts->tv_nsec = fftime_nsec(t);
}
#endif

/** Get time value in microseconds. */
static FFINL uint64 fftime_mcs(const fftime *t) {
	return (uint64)t->sec * 1000000 + fftime_usec(t);
}

/** Get time value in milliseconds. */
static FFINL uint64 fftime_ms(const fftime *t) {
	return (uint64)t->sec * 1000 + fftime_msec(t);
}


/** Convert between Windows FILETIME structure & fftime. */
typedef struct fftime_winftime {
	uint lo, hi;
} fftime_winftime;

enum { FFTIME_100NS = 116444736000000000ULL }; //100-ns intervals within 1600..1970

static FFINL fftime fftime_from_winftime(const fftime_winftime *ft)
{
	fftime t = {0};
	uint64 i = ((uint64)ft->hi << 32) | ft->lo;
	if (i > FFTIME_100NS) {
		i -= FFTIME_100NS;
		fftime_setsec(&t, i / (1000000 * 10));
		fftime_setnsec(&t, (i % (1000000 * 10)) * 100);
	}
	return t;
}

static FFINL void fftime_to_winftime(const fftime *t, fftime_winftime *ft)
{
	uint64 d = fftime_sec(t) * 1000000 * 10 + fftime_nsec(t) / 100 + FFTIME_100NS;
	ft->lo = (uint)d;
	ft->hi = (uint)(d >> 32);
}

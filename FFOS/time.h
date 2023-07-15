/** ffos: date & time
2020, Simon Zolin
*/

/*
fftime_now
fftime_local
fftime_empty
fftime_null
Get time parts:
	fftime_sec fftime_msec fftime_usec fftime_nsec
Convert:
	fftime_to_msec fftime_to_usec
	fftime_from_time_t fftime_to_time_t
	fftime_from_timeval fftime_to_timeval
	fftime_from_timespec fftime_to_timespec
	fftime_from_winftime fftime_to_winftime
*/

#pragma once

#include <FFOS/base.h>
static int fferr_str(int code, char *buffer, ffsize cap);
#include <ffbase/time.h>
#include <time.h>

typedef struct fftime_zone {
	int off; // UTC+X (in seconds)
	ffuint have_dst; // may have daylight saving time rules
	ffuint is_dst; // daylight saving time is active
	int real_offset; // offset (seconds) considering DST
} fftime_zone;

#define fftime_empty(t)  ((t)->sec == 0 && (t)->nsec == 0)

static inline void fftime_null(fftime *t)
{
	t->sec = 0,  t->nsec = 0;
}


#define fftime_sec(t)  ((t)->sec)
#define fftime_msec(t)  ((t)->nsec / 1000000)
#define fftime_usec(t)  ((t)->nsec / 1000)
#define fftime_nsec(t)  ((t)->nsec)


static inline void fftime_setsec(fftime *t, ffuint s)
{
	t->sec = s;
}
static inline void fftime_setmsec(fftime *t, ffuint ms)
{
	t->nsec = ms * 1000000;
}
static inline void fftime_setusec(fftime *t, ffuint us)
{
	t->nsec = us * 1000;
}
static inline void fftime_setnsec(fftime *t, ffuint ns)
{
	t->nsec = ns;
}


/** Get time value in milliseconds */
static inline ffuint64 fftime_to_msec(const fftime *t)
{
	return t->sec * 1000 + t->nsec / 1000000;
}

/** Get time value in microseconds */
static inline ffuint64 fftime_to_usec(const fftime *t)
{
	return t->sec * 1000000 + t->nsec / 1000;
}


/** time_t -> fftime */
static inline fftime fftime_from_time_t(time_t tt)
{
	fftime t = {
		.sec = tt,
		.nsec = 0,
	};
	return t;
}

/** fftime -> time_t */
static inline time_t fftime_to_time_t(const fftime *t)
{
	return t->sec;
}

#ifdef FF_UNIX

/** timeval -> fftime */
static inline fftime fftime_from_timeval(const struct timeval *tv)
{
	fftime t = {
		.sec = tv->tv_sec,
		.nsec = (ffuint)tv->tv_usec * 1000,
	};
	return t;
}

/** fftime -> timeval */
static inline struct timeval fftime_to_timeval(const fftime *t)
{
	struct timeval tv = {
		.tv_sec = t->sec,
		.tv_usec = t->nsec / 1000,
	};
	return tv;
}

/** timespec -> fftime */
static inline fftime fftime_from_timespec(const struct timespec *ts)
{
	fftime t = {
		.sec = ts->tv_sec,
		.nsec = (ffuint)ts->tv_nsec,
	};
	return t;
}

/** fftime -> timespec */
static inline struct timespec fftime_to_timespec(const fftime *t)
{
	struct timespec ts = {
		.tv_sec = t->sec,
		.tv_nsec = t->nsec,
	};
	return ts;
}

#endif

typedef struct fftime_winftime {
	ffuint lo, hi;
} fftime_winftime;

#define FFTIME_100NS  116444736000000000ULL // 100-ns intervals within 1600..1970

/** Windows FILETIME -> fftime */
static inline fftime fftime_from_winftime(const fftime_winftime *ft)
{
	fftime t = {};
	ffuint64 i = ((ffuint64)ft->hi << 32) | ft->lo;
	if (i > FFTIME_100NS) {
		i -= FFTIME_100NS;
		t.sec = i / (1000000 * 10);
		t.nsec = (i % (1000000 * 10)) * 100;
	}
	return t;
}

/** fftime -> Windows FILETIME */
static inline fftime_winftime fftime_to_winftime(const fftime *t)
{
	ffuint64 d = t->sec * 1000000 * 10 + t->nsec / 100 + FFTIME_100NS;
	fftime_winftime ft = {
		.lo = (ffuint)d,
		.hi = (ffuint)(d >> 32),
	};
	return ft;
}


#ifdef FF_WIN

static inline void fftime_now(fftime *t)
{
	FILETIME ft;
#if FF_WIN >= 0x0600
	GetSystemTimePreciseAsFileTime(&ft);
#else
	typedef VOID WINAPI (*_GetSystemTimePreciseAsFileTime_t)(LPFILETIME);
	static _GetSystemTimePreciseAsFileTime_t _GetSystemTimePreciseAsFileTime = (_GetSystemTimePreciseAsFileTime_t)-1;
	if (_GetSystemTimePreciseAsFileTime == (_GetSystemTimePreciseAsFileTime_t)-1)
		_GetSystemTimePreciseAsFileTime = (_GetSystemTimePreciseAsFileTime_t)(void*)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetSystemTimePreciseAsFileTime");
	if (_GetSystemTimePreciseAsFileTime != NULL)
		_GetSystemTimePreciseAsFileTime(&ft);
	else
		GetSystemTimeAsFileTime(&ft);
#endif
	*t = fftime_from_winftime((fftime_winftime*)&ft);
}

static inline void fftime_local(fftime_zone *tz)
{
	tzset();
	tz->off = -timezone;
	tz->have_dst = daylight;

	struct tm tm;
	time_t gt = time(NULL);
	gmtime_s(&tm, &gt);
	tm.tm_isdst = -1;
	time_t lt = mktime(&tm);
	tz->is_dst = tm.tm_isdst;
	tz->real_offset = gt - lt;
}

#else

/** Get UTC time (since Jan 1 1970) */
static inline void fftime_now(fftime *t)
{
	struct timespec ts = {};
	(void)clock_gettime(CLOCK_REALTIME, &ts);
	*t = fftime_from_timespec(&ts);
}

/** Get local timezone */
static inline void fftime_local(fftime_zone *tz)
{
	time_t gt = time(NULL);
	struct tm tm;

#ifdef FF_LINUX
	tzset();
	tz->off = -timezone;
	tz->have_dst = daylight;

#else
	tzset();
	localtime_r(&gt, &tm);
	tz->off = tm.tm_gmtoff;
	tz->have_dst = 0;
#endif

	gmtime_r(&gt, &tm);
	tm.tm_isdst = -1;
	time_t lt = mktime(&tm);
	tz->is_dst = tm.tm_isdst;
	tz->real_offset = gt - lt;
}

#endif

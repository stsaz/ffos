/**
Date & time.
Copyright (c) 2013 Simon Zolin
*/

#pragma once

#include <FFOS/types.h>

#include <time.h>

typedef struct {
	uint s;
	uint mcs;
} fftime;

enum FF_TIMEZONE {
	FFTIME_TZUTC
	, FFTIME_TZLOCAL
};

typedef union {
	struct {
		unsigned year : 16 ///< 0..4095
			, month : 5 ///< 1..12
			, weekday : 3	///< 0..6
			, day : 8 ///< 1..31

			, hour : 8 ///< 0..23
			, min : 8 ///< 0..59
			, sec : 6 ///< 0..59
			, msec : 10 ///< 0..999
			;
	};
	struct {
		uint date;
		uint time;
	};
} ffdtm;

static FFINL void fftime_null(fftime *t) {
	t->s = t->mcs = 0;
}

/** Get UTC time. */
FF_EXTN void fftime_now(fftime *t);

/** Split the time value into date and time elements. */
FF_EXTN void fftime_split(ffdtm *dt, const fftime *t, enum FF_TIMEZONE tz);

/** Join the time parts. */
FF_EXTN fftime * fftime_join(fftime *t, const ffdtm *dt, enum FF_TIMEZONE tz);

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

/** Convert 'ffdtm' to 'tm'. */
FF_EXTN void fftime_totm(struct tm *tt, const ffdtm *dt);

/** Convert 'tm' to 'ffdtm'. */
FF_EXTN void fftime_fromtm(ffdtm *dt, const struct tm *tt);

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

/** Get time value in microseconds. */
static FFINL uint64 fftime_mcs(const fftime *t) {
	return (uint64)t->s * 1000000 + t->mcs;
}

/** Get time value in milliseconds. */
static FFINL uint64 fftime_ms(const fftime *t) {
	return (uint64)t->s * 1000 + t->mcs / 1000;
}

/** Compare two 'fftime' objects. */
FF_EXTN int fftime_cmp(const fftime *t1, const fftime *t2);

/** Get time difference. */
FF_EXTN void fftime_diff(const fftime *start, fftime *stop);

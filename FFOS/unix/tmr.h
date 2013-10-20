/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/time.h>

#if defined FF_LINUX
#include <FFOS/unix/tmr-linux.h>
#elif defined FF_BSD
#include <FFOS/unix/tmr-bsd.h>
#endif

/** Get clock value. */
static FFINL int ffclk_get(fftime *result) {
	struct timespec ts;
	int r = clock_gettime(CLOCK_MONOTONIC, &ts);
	if (r == 0)
		fftime_fromtimespec(result, &ts);
	return r;
}

/** Get clock difference. */
#define ffclk_diff  fftime_diff

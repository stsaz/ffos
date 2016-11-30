/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/time.h>


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

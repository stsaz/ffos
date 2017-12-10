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
static FFINL void ffclk_diff(const fftime *start, fftime *stop)
{
	stop->sec -= start->sec;
	stop->nsec -= start->nsec;
	if ((int)stop->nsec < 0) {
		stop->nsec += 1000000000;
		stop->sec--;
	}
}

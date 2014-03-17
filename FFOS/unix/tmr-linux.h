/**
Copyright (c) 2013 Simon Zolin
*/

#include <sys/timerfd.h>

typedef fffd fftmr;

#define FF_BADTMR  FF_BADFD

/** Create timer.
Return FF_BADTMR on error. */
static FFINL fftmr fftmr_create(int flags) {
	return timerfd_create(CLOCK_MONOTONIC, flags);
}

static FFINL int fftmr_read(fftmr tmr) {
	int64 val;
	ssize_t r = read(tmr, &val, sizeof(int64));
	if (r != sizeof(int64))
		return -1;
	return 0;
}

/** Stop a timer. */
static FFINL int fftmr_stop(fftmr tmr, fffd qu) {
	const struct itimerspec ts = { {0, 0}, {0, 0} };
	return timerfd_settime(tmr, 0, &ts, NULL);
}

/** Close a timer. */
#define fftmr_close(tmr, qu)  close(tmr)

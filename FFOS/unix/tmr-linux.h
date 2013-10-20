/**
Copyright (c) 2013 Simon Zolin
*/

#include <sys/timerfd.h>

typedef fffd fftmr;

#define FF_BADTMR  FF_BADFD

/** Create timer.
Return FF_BADTMR on error. */
static FFINL fffd fftmr_create(int flags) {
	return timerfd_create(CLOCK_MONOTONIC, flags);
}

FF_EXTN int fftmr_start(fffd tmr, fffd qu, void *data, int periodMs);

static FFINL int fftmr_read(fffd tmr) {
	int64 val;
	ssize_t r = read(tmr, &val, sizeof(int64));
	if (r != sizeof(int64))
		return -1;
	return 0;
}

static FFINL int fftmr_stop(fffd tmr, fffd qu) {
	const struct itimerspec ts = { {0, 0}, {0, 0} };
	return timerfd_settime(tmr, 0, &ts, NULL);
}

#define fftmr_close(tmr, qu)  close(tmr)

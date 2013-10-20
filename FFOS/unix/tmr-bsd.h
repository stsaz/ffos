/**
Copyright (c) 2013 Simon Zolin
*/

#include <sys/event.h>
#include <stdlib.h>

typedef ssize_t fftmr;

#define FF_BADTMR  FF_BADFD

static FFINL fftmr fftmr_create(int flags) {
	void *t = malloc(1);
	if (t == NULL)
		return FF_BADTMR;
	return (ssize_t)t;
}

FF_EXTN int fftmr_start(fftmr tmr, fffd qu, void *data, int periodMs);

#define fftmr_read(tmr)

static FFINL int fftmr_stop(fftmr tmr, fffd qu) {
	struct kevent kev;
	EV_SET(&kev, tmr, EVFILT_TIMER, EV_DISABLE, 0, 0, (void*)-1);
	return kevent(qu, &kev, 1, NULL, 0, NULL);
}

static FFINL int fftmr_close(fftmr tmr, fffd qu) {
	struct kevent kev;
	EV_SET(&kev, tmr, EVFILT_TIMER, EV_DELETE, 0, 0, (void*)-1);
	(void)kevent(qu, &kev, 1, NULL, 0, NULL);
	free((void*)tmr);
	return 0;
}

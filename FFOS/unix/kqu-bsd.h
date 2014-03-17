/**
Copyright (c) 2013 Simon Zolin
*/

#include <sys/event.h>
#include <errno.h>


enum FFKQU_F {
	FFKQU_ADD = EV_ADD
	, FFKQU_DEL = EV_DELETE

	, FFKQU_READ = -(EVFILT_READ) << 24
	, FFKQU_WRITE = -(EVFILT_WRITE) << 24

	, FFKQU_ERR = EV_ERROR
	, FFKQU_EOF = EV_EOF
};

typedef struct kevent ffkqu_entry;

#define ffkqu_data(ev)  ((ev)->udata)

static FFINL int ffkqu_result(const ffkqu_entry *e, fffd fd) {
	(void)fd;
	int r = FFKQU_READ;
	if (e->flags & EV_ERROR)
		errno = e->data;

	if (e->filter == EVFILT_WRITE)
		r = FFKQU_WRITE;
	return r | e->flags;
}

#define ffkqu_create  kqueue

static FFINL int ffkqu_attach(fffd kq, fffd fd, void *udata, int flags) {
	struct kevent evs[2];
	int i = 0;
	int f = (flags & 0x00ffffff) | EV_CLEAR;

	if (flags & FFKQU_READ) {
		EV_SET(&evs[i], fd, EVFILT_READ, f, 0, 0, udata);
		i++;
	}
	if (flags & FFKQU_WRITE) {
		EV_SET(&evs[i], fd, EVFILT_WRITE, f, 0, 0, udata);
		i++;
	}

	return kevent(kq, evs, i, NULL, 0, NULL);
}

typedef struct timespec ffkqu_time;

static FFINL const ffkqu_time * ffkqu_settm(ffkqu_time *t, uint ms) {
	if (ms == (uint)-1)
		return NULL;
	t->tv_sec = ms / 1000;
	t->tv_nsec = (ms % 1000) * 1000000;
	return t;
}

#define ffkqu_runtimer()

#define ffkqu_wait(kq, events, eventsSize, tmout)\
	kevent((kq), NULL, 0, (events), FF_TOINT(eventsSize), (tmout))

#define ffkqu_close close

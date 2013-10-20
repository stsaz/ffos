/**
Copyright (c) 2013 Simon Zolin
*/

#include <sys/event.h>

typedef struct kevent ffkqu_entry;

#define ffkqu_data(ev)  ((ev)->udata)

#define ffkqu_create  kqueue

typedef struct timespec ffkqu_time;

static FFINL const ffkqu_time * ffkqu_settm(ffkqu_time *t, uint ms) {
	if (ms == (uint)-1)
		return NULL;
	t->tv_sec = ms / 1000;
	t->tv_nsec = (ms % 1000) * 1000000;
	return t;
}

#define ffkqu_wait(kq, events, eventsSize, tmout)\
	kevent((kq), NULL, 0, (events), FF_TOINT(eventsSize), (tmout))

#define ffkqu_close close

/**
Copyright (c) 2013 Simon Zolin
*/

#include <sys/epoll.h>

typedef struct epoll_event ffkqu_entry;

#define ffkqu_data(ev)  ((ev)->data.ptr)

/** Create kernel queue.
Return FF_BADFD on error. */
#define ffkqu_create()  epoll_create(1)

typedef uint ffkqu_time;

/** Set timeout value which should be passed into ffkqu_wait(). */
static FFINL const ffkqu_time * ffkqu_settm(ffkqu_time *t, uint ms) {
	*t = ms;
	return t;
}

/** Wait for an event from kernel.
Return the number of signaled events.
Return 0 on timeout.
Return -1 on error. */
#define ffkqu_wait(kq, events, eventsSize, /* const ffkqu_time* */ tmoutMs)\
	epoll_wait((kq), (events), FF_TOINT(eventsSize), *(tmoutMs));

/** Close kernel queue. */
#define ffkqu_close  close

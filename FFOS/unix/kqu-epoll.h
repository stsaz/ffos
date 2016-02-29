/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/socket.h>

#include <sys/epoll.h>
#include <errno.h>


enum FFKQU_F {
	FFKQU_ADD = EPOLL_CTL_ADD << 24
	, FFKQU_DEL = EPOLL_CTL_DEL << 24

	, FFKQU_READ = EPOLLIN
	, FFKQU_WRITE = EPOLLOUT
	, FFKQU_ERR = EPOLLERR
	, FFKQU_EOF = EPOLLHUP
};

typedef struct epoll_event ffkqu_entry;

/** Get user data from the kernel event structure. */
#define ffkqu_data(ev)  ((ev)->data.ptr)

/** Get signaled events and flags.
Set the last socket error in 'errno' if needed. */
static FFINL int ffkqu_result(const ffkqu_entry *e, fffd fd) {
	if (e->events & EPOLLERR) {
		int er;
		(void)ffskt_getopt(fd, SOL_SOCKET, SO_ERROR, &er);
		errno = er;
		return e->events | FFKQU_READ | FFKQU_WRITE;
	}
	return e->events;
}

/** Create kernel queue.
Return FF_BADFD on error. */
#define ffkqu_create()  epoll_create(1)

/** Attach fd to kernel queue. */
static FFINL int ffkqu_attach(fffd kq, fffd fd, void *udata, int flags) {
	int ctl = (flags >> 24);
	struct epoll_event e;
	e.events = (flags & 0x00ffffff) | EPOLLET;
	e.data.ptr = udata;
	return epoll_ctl(kq, ctl, fd, &e);
}

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
	epoll_wait((kq), (events), FF_TOINT(eventsSize), *(tmoutMs))

/** Close kernel queue. */
#define ffkqu_close  close

/** Init/uninit eventfd for ffkqu_post().
@kq: kqueue to attach eventfd to;  FF_BADFD: close eventfd. */
FF_EXTN int ffkqu_post_attach(fffd kq);

FF_EXTN int ffkqu_post(fffd kq, void *data, void *unused);

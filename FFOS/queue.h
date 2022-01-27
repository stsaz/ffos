/** ffos: kernel queue: kqueue, epoll, IOCP
2020, Simon Zolin
*/

/*
ffkq_create ffkq_close
ffkq_attach ffkq_attach_socket
ffkq_time_set
Event:
	ffkq_event_data
	ffkq_event_flags
	ffkq_event_result_socket
User event:
	ffkq_post_attach
	ffkq_post_detach
	ffkq_post
	ffkq_post_consume
*/

#pragma once

#include <FFOS/base.h>

#ifdef FF_WIN

typedef HANDLE ffkq;
#define FFKQ_NULL  NULL
typedef ffuint ffkq_time;
typedef HANDLE ffkq_postevent;
typedef OVERLAPPED_ENTRY ffkq_event;

static inline ffkq ffkq_create()
{
	return CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
}

static inline void ffkq_close(ffkq kq)
{
	CloseHandle(kq);
}

enum FFKQ_ATTACH {
	FFKQ_READ = 1,
	FFKQ_WRITE = 2,
	FFKQ_READWRITE = FFKQ_READ | FFKQ_WRITE,
};

static inline int ffkq_attach(ffkq kq, HANDLE fd, void *data, int flags)
{
	(void)flags;
	return !CreateIoCompletionPort(fd, kq, (ULONG_PTR)data, 0);
}

static inline int ffkq_attach_socket(ffkq kq, SOCKET sk, void *data, int flags)
{
	(void)flags;
	return !CreateIoCompletionPort((HANDLE)sk, kq, (ULONG_PTR)data, 0);
}

#if FF_WIN >= 0x0600

static inline int ffkq_wait(ffkq kq, ffkq_event *events, ffuint events_cap, ffkq_time timeout)
{
	ULONG n = 0;
	BOOL b = GetQueuedCompletionStatusEx(kq, events, events_cap, &n, timeout, 0);
	if (!b)
		return (GetLastError() == WAIT_TIMEOUT) ? 0 : -1;
	return (int)n;
}

#else // old Windows:

static inline int ffkq_wait(ffkq kq, ffkq_event *events, ffuint events_cap, ffkq_time timeout)
{
	if (events_cap == 0)
		return 0;

	BOOL b = GetQueuedCompletionStatus(kq, &events->dwNumberOfBytesTransferred
		, &events->lpCompletionKey, &events->lpOverlapped, timeout);
	if (!b) {
		if (GetLastError() == WAIT_TIMEOUT)
			return 0;
		return (events->lpOverlapped == NULL) ? -1 : 1;
	}
	return 1;
}

#endif

static inline void ffkq_time_set(ffkq_time *t, ffuint msec)
{
	*t = msec;
}

static inline void* ffkq_event_data(ffkq_event *ev)
{
	return (void*)ev->lpCompletionKey;
}

static inline int ffkq_event_flags(ffkq_event *ev)
{
	(void)ev;
	return 0;
}

static inline int ffkq_event_result_socket(ffkq_event *ev, int sk)
{
	(void)ev; (void)sk;
	return 0;
}


static inline ffkq_postevent ffkq_post_attach(ffkq kq, void *data)
{
	(void)data;
	return kq;
}

static inline void ffkq_post_detach(ffkq_postevent post, ffkq kq)
{
	(void)post; (void)kq;
}

static inline int ffkq_post(ffkq_postevent post, void *data)
{
	ffkq kq = post;
	return !PostQueuedCompletionStatus(kq, 0, (ULONG_PTR)data, NULL);
}

static inline void ffkq_post_consume(ffkq_postevent post)
{
	(void)post;
}

#else // UNIX:

typedef int ffkq;
typedef int ffkq_postevent;
#define FFKQ_NULL  (-1)

#ifdef FF_LINUX

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/errno.h>
#include <sys/socket.h>

typedef struct epoll_event ffkq_event;
typedef ffuint ffkq_time;

static inline ffkq ffkq_create()
{
	return epoll_create(1);
}

enum FFKQ_ATTACH {
	FFKQ_READ = EPOLLIN,
	FFKQ_WRITE = EPOLLOUT,
	FFKQ_READWRITE = EPOLLIN | EPOLLOUT,
};

static inline int ffkq_attach(ffkq kq, int fd, void *data, int flags)
{
	struct epoll_event e;
	e.events = flags | EPOLLET;
	e.data.ptr = data;
	return epoll_ctl(kq, EPOLL_CTL_ADD, fd, &e);
}

#define ffkq_attach_socket  ffkq_attach

static inline int ffkq_wait(ffkq kq, ffkq_event *events, ffuint events_cap, ffkq_time timeout)
{
	return epoll_wait(kq, events, events_cap, timeout);
}

static inline void ffkq_time_set(ffkq_time *t, ffuint msec)
{
	*t = msec;
}

static inline void* ffkq_event_data(ffkq_event *ev)
{
	return ev->data.ptr;
}

static inline int ffkq_event_flags(ffkq_event *ev)
{
	return !(ev->events & EPOLLERR) ? ev->events : (int)FFKQ_READWRITE;
}

static inline int ffkq_event_result_socket(ffkq_event *ev, int sk)
{
	int err = 0;
	if (ev->events & EPOLLERR) {
		socklen_t len = sizeof(int);
		if (0 != getsockopt(sk, SOL_SOCKET, SO_ERROR, &err, &len))
			err = errno;
	}
	return err;
}


static inline ffkq_postevent ffkq_post_attach(ffkq kq, void *data)
{
	ffkq_postevent post;
	if (-1 == (post = eventfd(0, EFD_NONBLOCK)))
		return FFKQ_NULL;

	if (0 != ffkq_attach(kq, post, data, FFKQ_READ)) {
		close(post);
		return FFKQ_NULL;
	}

	return post;
}

static inline void ffkq_post_detach(ffkq_postevent post, ffkq kq)
{
	(void)kq;
	close(post);
}

static inline int ffkq_post(ffkq_postevent post, void *data)
{
	ffuint64 val = (ffsize)data;
	if (sizeof(ffuint64) != write(post, &val, sizeof(ffuint64)))
		return -1;
	return 0;
}

static inline void ffkq_post_consume(ffkq_postevent post)
{
	for (;;) {
		ffuint64 val;
		int r = read(post, &val, sizeof(ffuint64));
		if (r < 0)
			break;
	}
}

#else // BSD/macOS:

#include <sys/event.h>

typedef struct kevent ffkq_event;
typedef struct timespec ffkq_time;
#define _FFKQ_POST_ID  1

static inline ffkq ffkq_create()
{
	return kqueue();
}

enum FFKQ_ATTACH {
	FFKQ_READ = 1,
	FFKQ_WRITE = 2,
	FFKQ_READWRITE = FFKQ_READ | FFKQ_WRITE,
};

static inline int ffkq_attach(ffkq kq, int fd, void *data, int flags)
{
	struct kevent evs[2];
	int i = 0;

	if (flags & FFKQ_READ) {
		EV_SET(&evs[i], fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, data);
		i++;
	}
	if (flags & FFKQ_WRITE) {
		EV_SET(&evs[i], fd, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, data);
		i++;
	}

	return kevent(kq, evs, i, NULL, 0, NULL);
}

#define ffkq_attach_socket  ffkq_attach

static inline int ffkq_wait(ffkq kq, ffkq_event *events, ffuint events_cap, ffkq_time timeout)
{
	return kevent(kq, NULL, 0, events, events_cap, &timeout);
}

static inline void ffkq_time_set(ffkq_time *t, ffuint msec)
{
	t->tv_sec = msec / 1000;
	t->tv_nsec = (msec % 1000) * 1000000;
}

static inline void* ffkq_event_data(ffkq_event *ev)
{
	return ev->udata;
}

static inline int ffkq_event_flags(ffkq_event *ev)
{
	return (ev->filter != EVFILT_WRITE) ? FFKQ_READ : FFKQ_WRITE;
}

static inline int ffkq_event_result_socket(ffkq_event *ev, int sk)
{
	(void)sk;
	int err = 0;
	if (ev->flags & EV_ERROR)
		err = ev->data;
	else if (ev->filter == EVFILT_WRITE && (ev->flags & EV_EOF))
		err = ev->fflags;
	return err;
}


static inline ffkq_postevent ffkq_post_attach(ffkq kq, void *data)
{
	(void)data;
	struct kevent ev;
	EV_SET(&ev, _FFKQ_POST_ID, EVFILT_USER, EV_ADD | EV_ENABLE | EV_CLEAR, 0, 0, NULL);
	if (0 != kevent(kq, &ev, 1, NULL, 0, NULL))
		return FFKQ_NULL;
	return kq;
}

static inline void ffkq_post_detach(ffkq_postevent post, ffkq kq)
{
	FF_ASSERT(post == kq);
	(void)post;
	struct kevent ev;
	EV_SET(&ev, _FFKQ_POST_ID, EVFILT_USER, EV_DELETE, 0, 0, NULL);
	kevent(kq, &ev, 1, NULL, 0, NULL);
}

static inline int ffkq_post(ffkq_postevent post, void *data)
{
	ffkq kq = post;
	struct kevent ev;
	EV_SET(&ev, _FFKQ_POST_ID, EVFILT_USER, 0, NOTE_TRIGGER, 0, data);
	return kevent(kq, &ev, 1, NULL, 0, NULL);
}

static inline void ffkq_post_consume(ffkq_postevent post)
{
	(void)post;
}

#endif // #ifdef FF_LINUX

static inline void ffkq_close(ffkq kq)
{
	close(kq);
}

#endif


/** Create kernel queue
Return FFKQ_NULL on error */
static ffkq ffkq_create();

/** Close kernel queue */
static void ffkq_close(ffkq kq);

/** Attach fd to kernel queue
flags: enum FFKQ_ATTACH
Return 0 on success */
static int ffkq_attach(ffkq kq, fffd fd, void *data, int flags);

/** Wait for an event from kernel
UNIX: may be interrupted by a signal (EINTR)
timeout:
  >0: wait for events
  0: don't wait and return immediately
Return the number of signaled events
  0 on timeout
  <0 on error */
static int ffkq_wait(ffkq kq, ffkq_event *events, ffuint events_cap, ffkq_time timeout);

/** Set timeout value which should be passed to ffkq_wait() */
static void ffkq_time_set(ffkq_time *t, ffuint msec);

/** Get user data from kernel event object */
static void* ffkq_event_data(ffkq_event *ev);

/** Get event flags from kernel event object */
static int ffkq_event_flags(ffkq_event *ev);

/** Get error code from kernel event object associated with a socket */
static int ffkq_event_result_socket(ffkq_event *ev, int sk);


/** Attach user event to kqueue
BSD/macOS: only one user event per kqueue is supported
Return FFKQ_NULL on error */
static ffkq_postevent ffkq_post_attach(ffkq kq, void *data);

/** Detach user event from kqueue */
static void ffkq_post_detach(ffkq_postevent post, ffkq kq);

/** Trigger user event
data: must be the same value as was used for ffkq_post_attach()
  Linux: this value is NOT what ffkq_event_data() will return (see ffkq_post()/ffkq_post_consume())
UNIX: several ffkq_post() calls trigger kqueue event only once */
static int ffkq_post(ffkq_postevent post, void *data);

/** Consume data from user event: must be called on each signal */
static void ffkq_post_consume(ffkq_postevent post);

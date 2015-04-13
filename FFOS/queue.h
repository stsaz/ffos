/**
Kernel queue.  kqueue, epoll, I/O completion ports.
Copyright (c) 2013 Simon Zolin
*/

#pragma once

#include <FFOS/socket.h>

#if defined FF_BSD
	#include <FFOS/unix/kqu-bsd.h>
#elif defined FF_LINUX
	#include <FFOS/unix/kqu-epoll.h>
#elif defined FF_WIN
	#include <FFOS/win/kqu-iocp.h>
#endif


typedef void (*ffkev_handler)(void *udata);

/** Connector between kernel events and user-space event handlers. */
typedef struct ffkevent {
	ffkev_handler handler;
	void *udata;
	union {
		fffd fd;
		ffskt sk;
	};
	unsigned side :1
		, oneshot :1 //"handler" is set to NULL each time the event signals
		, aiotask :1 //compatibility with ffaio_task
		, pending :1
		;
#ifndef FF_WIN
	ffkqu_entry *ev;
#endif

#if defined FF_WIN
	OVERLAPPED ovl;
#endif
} ffkevent;

/** Initialize kevent. */
static FFINL void ffkev_init(ffkevent *kev)
{
	unsigned side = kev->side;
	memset(kev, 0, sizeof(ffkevent));
	kev->side = side;
	kev->oneshot = 1;
	kev->fd = FF_BADFD;
}

/** Finish working with kevent. */
static FFINL void ffkev_fin(ffkevent *kev)
{
	kev->side = !kev->side;
	kev->handler = NULL;
	kev->udata = NULL;
	kev->fd = FF_BADFD;
}

/** Get udata to register in the kernel queue. */
static FFINL void* ffkev_ptr(ffkevent *kev)
{
	return (void*)((size_t)kev | kev->side);
}

/** Attach kevent to kernel queue. */
#define ffkev_attach(kev, kq, flags) \
	ffkqu_attach(kq, (kev)->fd, ffkev_ptr(kev), FFKQU_ADD | (flags))

/** Call an event handler. */
FF_EXTN void ffkev_call(ffkqu_entry *e);

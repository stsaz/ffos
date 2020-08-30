/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/file.h>
#include <FFOS/dir.h>
#include <FFOS/time.h>
#include <FFOS/socket.h>
#include <FFOS/error.h>
#include <FFOS/asyncio.h>
#include <FFOS/atomic.h>
#include <FFOS/process.h>
#include <FFOS/sig.h>
#include <FFOS/std.h>
#include <ffbase/string.h>
#include <assert.h>

#include <FFOS/ffos-extern.h>

#if !defined FF_WIN || FF_WIN >= 0x0600
void fftime_init(void)
{}
#endif


size_t ffiov_shiftv(ffiovec *iovs, size_t nels, uint64 *len)
{
	size_t i;
	for (i = 0;  i != nels;  i++) {
		if (*len < iovs[i].iov_len) {
			ffiov_shift(&iovs[i], (size_t)*len);
			*len = 0;
			break;
		}
		*len -= iovs[i].iov_len;
	}
	return i;
}

size_t ffiov_copyhdtr(ffiovec *dst, size_t cap, const sf_hdtr *hdtr)
{
	size_t n = 0;
	int i;
	for (i = 0; i < hdtr->hdr_cnt && n < cap; i++) {
		dst[n++] = hdtr->headers[i];
	}
	for (i = 0; i < hdtr->trl_cnt && n < cap; i++) {
		dst[n++] = hdtr->trailers[i];
	}
	return n;
}


/*
AIO:
1. ffaio_attach()
2. ffaio_*() -> non-blocking I/O -> EAGAIN -> set ffaio_task.handler, begin async operation (Windows)
3. signal from kqueue/epoll/IOCP -> ffkev_call() -> ffaio_task.handler() <-> ffaio_*() -> real I/O

Cancellation on UNIX:
ffaio_cancelasync() -> ffaio_task.handler() <-> ffaio_*() (errno=ECANCELED)

Cancellation on Windows:
1. ffaio_cancelasync() -> cancel async operation
2. signal from IOCP -> ffkev_call() -> ffaio_task.handler() <-> ffaio_*() (errno=ECANCELED)
*/

static void _ffaio_run1(ffkqu_entry *e)
{
	size_t udata = (size_t)ffkqu_data(e);
	ffaio_task *t = (void*)(udata & ~1);
	ffaio_handler func;
	uint r, w;

	uint evflags = _ffaio_events(t, e);
	r = 0 != (evflags & FFKQU_READ);
	w = 0 != (evflags & FFKQU_WRITE);

	FFDBG_PRINTLN(FFDBG_KEV | 10, "task:%p, fd:%L, evflags:%xu, r:%u, w:%u, rhandler:%p, whandler:%p, stale:%u"
		, t, (size_t)t->fd, (int)evflags, r, w, t->rhandler, t->whandler, (udata & 1) != t->instance);

	if ((udata & 1) != t->instance)
		return; //cached event has signaled

#ifdef FF_LINUX
/* In epoll both R & W flags may be signalled together.
Don't call whandler() here in case it's initially NULL, but then is set inside rhandler().
Otherwise EPOLLERR flag may be reported incorrectly. */

	ffaio_handler wfunc = t->whandler;

	if (r && t->rhandler != NULL) {
		func = t->rhandler;
		t->rhandler = NULL;
		t->ev = e;
		func(t->udata);
		// 'whandler' may be modified
	}

	if (w && wfunc != NULL && wfunc == t->whandler) {
		func = t->whandler;
		t->whandler = NULL;
		t->ev = e;
		func(t->udata);
	}

	return;
#endif

	if (r && t->rhandler != NULL) {
		func = t->rhandler;
		t->rhandler = NULL;
		t->ev = e;
		func(t->udata);

	} else if (w && t->whandler != NULL) {
		func = t->whandler;
		t->whandler = NULL;
		t->ev = e;
		func(t->udata);
	}
}

void ffkev_call(ffkqu_entry *e)
{
	size_t udata = (size_t)ffkqu_data(e);
	ffkevent *kev = (void*)(udata & ~1);

	if (kev->aiotask) {
		_ffaio_run1(e);
		return;
	}

	uint evflags;
#if defined FF_LINUX
	evflags = e->events;
#elif defined FF_BSD || defined FF_APPLE
	evflags = e->flags;
#else
	evflags = 0;
#endif
	(void)evflags;
	FFDBG_PRINTLN(FFDBG_KEV | 10, "task:%p, fd:%L, evflags:%xu, handler:%p, stale:%u"
		, kev, (size_t)kev->fd, evflags, kev->handler, (udata & 1) != kev->side);

	if ((udata & 1) != kev->side) {
		/* kev.side is modified in ffkev_fin() every time an event is closed or reused for another operation.
		This event was cached in userspace and it's no longer valid. */
		return;
	}

	if (kev->handler != NULL) {
		ffkev_handler func = kev->handler;
		if (kev->oneshot)
			kev->handler = NULL;
		func(kev->udata);
	}
}


static const char *const err_ops[] = {
	"success"
	, "internal error"

	, "read"
	, "write"

	, "buffer alloc"
	, "buffer grow"

	, "file open"
	, "file seek"
	, "file map"
	, "file remove"
	, "file close"
	, "file rename"

	, "directory open"
	, "directory make"

	, "timer init"
	, "kqueue create"
	, "kqueue attach"
	, "kqueue wait"
	, "thread create"
	, "non-blocking mode set"
	, "process fork"

	, "library open"
	, "library symbol address"

	, "socket create"
	, "socket bind"
	, "socket option"
	, "socket shutdown"
	, "socket listen"
	, "socket connect"
	, "address resolve"

	, "system error"
};

const char * fferr_opstr(enum FFERR e)
{
	FF_ASSERT(e <= FFERR_SYSTEM);
	return err_ops[e];
}


int ffdbg_mask = 1;

int ffdbg_print(int t, const char *fmt, ...)
{
	(void)t;
	char buf[4096];
	ffstr s;
	ffstr_set(&s, buf, 0);

	static ffatomic counter;
	ffstr_addfmt(&s, sizeof(buf), "%p#%L "
		, &counter, (ffsize)ffatom_incret(&counter));

	va_list va;
	va_start(va, fmt);
	ffstr_addfmtv(&s, sizeof(buf), fmt, va);
	va_end(va);

	fffile_write(ffstdout, s.ptr, s.len);
	return 0;
}

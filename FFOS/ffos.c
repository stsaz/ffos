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


int ffdir_rmakeq(ffsyschar *path, size_t off)
{
	ffsyschar *s = path + off;

	if (ffpath_slash(*s))
		s++;

	for (;; s++) {

		if (ffpath_slash(*s) || *s == '\0') {
			int er;
			ffsyschar slash = *s;

			*s = '\0';
			er = ffdir_makeq(path);
			*s = slash;

			if (er != 0 && fferr_last() != EEXIST)
				return er;

			if (*s == '\0')
				break;
		}
	}

	return 0;
}


#ifdef FF_UNIX
void fftimespec_addms(struct timespec *ts, uint64 ms)
{
	ts->tv_sec += ms / 1000;
	ts->tv_nsec += (ms % 1000) * 1000 * 1000;
	if (ts->tv_nsec >= 1000 * 1000 * 1000) {
		ts->tv_sec++;
		ts->tv_nsec -= 1000 * 1000 * 1000;
	}
}
#endif

static void _fftime_sub(fftime *t, const fftime *t2)
{
	t->sec -= t2->sec;
	t->nsec -= t2->nsec;
	if ((int)t->nsec < 0) {
		t->nsec += 1000000000;
		t->sec--;
	}
}

#if !defined FF_WIN || FF_WIN >= 0x0600
void fftime_init(void)
{}
#endif


void ffps_perf_diff(const struct ffps_perf *start, struct ffps_perf *stop)
{
	_fftime_sub(&stop->realtime, &start->realtime);
	_fftime_sub(&stop->cputime, &start->cputime);
	_fftime_sub(&stop->usertime, &start->usertime);
	_fftime_sub(&stop->systime, &start->systime);
	stop->pagefaults -= start->pagefaults;
	stop->maxrss -= start->maxrss;
	stop->inblock -= start->inblock;
	stop->outblock -= start->outblock;
	stop->vctxsw -= start->vctxsw;
	stop->ivctxsw -= start->ivctxsw;
}


#if defined FF_APPLE || defined FF_OLDLIBC
ffskt ffskt_accept(ffskt listenSk, struct sockaddr *a, socklen_t *addrSize, int flags)
{
	ffskt sk = accept(listenSk, a, addrSize);
	if ((flags & SOCK_NONBLOCK) && sk != FF_BADSKT && 0 != ffskt_nblock(sk, 1)) {
		ffskt_close(sk);
		return FF_BADSKT;
	}
	return sk;
}
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

	FFDBG_PRINTLN(FFDBG_KEV | 10, "task:%p, fd:%I, evflags:%xu, r:%u, w:%u, rhandler:%p, whandler:%p, stale:%u"
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
	FFDBG_PRINTLN(FFDBG_KEV | 10, "task:%p, fd:%I, evflags:%xu, handler:%p, stale:%u"
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


uint _ffsc_ncpu;

void fflk_setup(void)
{
	ffsysconf sc;
	if (_ffsc_ncpu != 0)
		return;
	ffsc_init(&sc);
	_ffsc_ncpu = ffsc_get(&sc, _SC_NPROCESSORS_ONLN);
}

enum { FFLK_SPIN = 2048 };

void fflk_lock(fflock *lk)
{
	if (_ffsc_ncpu == 1) {
		for (;;) {
			if (fflk_trylock(lk))
				return;

			ffcpu_yield();
		}
	}

	for (;;) {
		if (fflk_trylock(lk))
			return;
		for (uint n = 0;  n != FFLK_SPIN;  n++) {
			ffcpu_pause();
			if (fflk_trylock(lk))
				return;
		}
		ffcpu_yield();
	}
}

uint ffatom_waitchange(uint *p, uint curval)
{
	uint r;

	if (_ffsc_ncpu == 1) {
		for (;;) {
			if (curval != (r = FF_READONCE(p)))
				return r;

			ffcpu_yield();
		}
	}

	for (;;) {
		if (curval != (r = FF_READONCE(p)))
			return r;
		for (uint n = 0;  n != FFLK_SPIN;  n++) {
			ffcpu_pause();
			if (curval != (r = FF_READONCE(p)))
				return r;
		}
		ffcpu_yield();
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


#ifdef FFMEM_DBG
static ffatomic _ffmem_total; //accumulated size of (re)allocated memory; NOT the currently allocated memory size
void* ffmem_alloc(size_t size)
{
	void *p = _ffmem_alloc(size);
	ffatom_add(&_ffmem_total, size);
	FFDBG_PRINTLN(FFDBG_MEM | 10, "p:%p, size:%L [%LK]", p, size, _ffmem_total / 1024);
	return p;
}

void* ffmem_calloc(size_t n, size_t sz)
{
	void *p = _ffmem_calloc(n, sz);
	ffatom_add(&_ffmem_total, n * sz);
	FFDBG_PRINTLN(FFDBG_MEM | 10, "p:%p, size:%L*%L [%LK]", p, n, sz, _ffmem_total / 1024);
	return p;
}

void* ffmem_realloc(void *ptr, size_t newsize)
{
	void *p;
	p = _ffmem_realloc(ptr, newsize);
	ffatom_add(&_ffmem_total, newsize);
	FFDBG_PRINTLN(FFDBG_MEM | 10, "p:%p, size:%L, oldp:%p [%LK]", p, newsize, ptr, _ffmem_total / 1024);
	return p;
}

void ffmem_free(void *ptr)
{
	FFDBG_PRINTLN(FFDBG_MEM | 10, "p:%p", ptr);
	_ffmem_free(ptr);
}

void* ffmem_align(size_t size, size_t align)
{
	void *p = _ffmem_align(size, align);
	ffatom_add(&_ffmem_total, size);
	FFDBG_PRINTLN(FFDBG_MEM | 10, "p:%p, size:%L, align:%L [%LK]", p, size, align, _ffmem_total / 1024);
	return p;
}

void ffmem_alignfree(void *ptr)
{
	FFDBG_PRINTLN(FFDBG_MEM | 10, "p:%p", ptr);
	_ffmem_alignfree(ptr);
}
#endif //FFMEM_DBG


#if defined _DEBUG && defined FF_FFOS_ONLY
int ffdbg_mask;
int ffdbg_print(int t, const char *fmt, ...)
{
	return 0;
}
#endif

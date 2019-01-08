/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/time.h>
#include <FFOS/dir.h>
#include <FFOS/error.h>
#include <FFOS/process.h>
#include <FFOS/sig.h>
#include <FFOS/timer.h>
#include <FFOS/socket.h>
#include <FFOS/asyncio.h>
#include <FFOS/thread.h>
#include <FFOS/atomic.h>

#include <time.h>
#include <signal.h>
#include <sys/sendfile.h>
#include <sys/syscall.h>
#include <sys/eventfd.h>


#ifdef FF_LINUX_MAINLINE
int fffile_settime(fffd fd, const fftime *last_write)
{
	struct timespec ts[2];
	fftime_to_timespec(last_write, &ts[0]);
	fftime_to_timespec(last_write, &ts[1]);
	return futimens(fd, ts);
}
#endif

int fffile_settimefn(const char *fn, const fftime *last_write)
{
	struct timespec ts[2];
	fftime_to_timespec(last_write, &ts[0]);
	fftime_to_timespec(last_write, &ts[1]);
	return utimensat(AT_FDCWD, fn, ts, 0);
}


int ffpipe_create2(fffd *rd, fffd *wr, uint flags)
{
	fffd fd[2];
	if (0 != pipe2(fd, flags))
		return -1;
	*rd = fd[0];
	*wr = fd[1];
	return 0;
}


#ifdef FF_LINUX_MAINLINE
int fferr_str(int code, char *dst, size_t dst_cap) {
	char *dst2;
	if (0 == dst_cap)
		return 0;
	dst2 = strerror_r(code, dst, dst_cap);
	if (dst2 != dst) {
		ssize_t len = ffmin(dst_cap - 1, strlen(dst2));
		memcpy(dst, dst2, len);
		dst[len] = '\0';
		return 0;
	}
	return 0;
}
#else
int fferr_str(int code, char *dst, size_t dst_cap)
{
	FF_ASSERT(0);
	return 0;
}
#endif


const char* ffps_filename(char *name, size_t cap, const char *argv0)
{
	int n = readlink("/proc/self/exe", name, cap);
	if (n >= 0) {
		name[n] = '\0';
		return name;
	}

	return _ffpath_real(name, cap, argv0);
}

ffthd_id ffthd_curid(void)
{
	return syscall(SYS_gettid);
}


void fftime_local(fftime_zone *tz)
{
	tzset();
	tz->off = -timezone;
	tz->have_dst = daylight;
}


#if defined FF_LINUX_MAINLINE
int fftmr_start(fftmr tmr, fffd kq, void *udata, int period_ms)
{
	struct itimerspec its;
	struct epoll_event e;
	unsigned neg = 0;

	e.events = EPOLLIN | EPOLLET;
	e.data.ptr = udata;
	if (0 != epoll_ctl(kq, EPOLL_CTL_ADD, tmr, &e)
		&& errno != EEXIST)
		return -1;

	if (period_ms < 0) {
		neg = 1;
		period_ms = -period_ms;
		its.it_interval.tv_sec = its.it_interval.tv_nsec = 0;
	}

	its.it_value.tv_sec = period_ms / 1000;
	its.it_value.tv_nsec = (period_ms % 1000) * 1000 * 1000;
	if (!neg)
		its.it_interval = its.it_value;

	return timerfd_settime(tmr, 0, &its, NULL);
}


ffskt ffskt_create(uint domain, uint type, uint protocol)
{
	return socket(domain, type, protocol);
}

int ffskt_sendfile(ffskt sk, fffd fd, uint64 offs, uint64 sz, sf_hdtr *hdtr, uint64 *_sent, int flags)
{
	int npush = 0;
	ssize_t res = 0;
	ssize_t r;
	uint64 sent = 0;
	(void)flags;

	if (hdtr != NULL && (hdtr->hdr_cnt | hdtr->trl_cnt) != 0
		&& 0 == ffskt_setopt(sk, IPPROTO_TCP, TCP_NOPUSH, 1))
		npush = 1;

	if (hdtr != NULL && hdtr->hdr_cnt != 0) {
		size_t hsz = ffiov_size(hdtr->headers, hdtr->hdr_cnt);
		r = ffskt_sendv(sk, hdtr->headers, hdtr->hdr_cnt);
		if (r == -1) {
			res = -1;
			goto end;
		}

		sent = r;
		if ((size_t)r != hsz)
			goto end;
	}

	if (sz != 0) {
		off64_t foffset = offs;
		r = sendfile64(sk, fd, &foffset, FF_TOSIZE(sz));
		if (r == -1) {
			res = -1;
			goto end;
		}

		sent += r;
		if ((uint64)r != sz)
			goto end;
	}

	if (hdtr != NULL && hdtr->trl_cnt != 0) {
		r = ffskt_sendv(sk, hdtr->trailers, hdtr->trl_cnt);
		if (r == -1) {
			res = -1;
			goto end;
		}

		sent += r;
	}

end:
	if (npush)
		(void)ffskt_setopt(sk, IPPROTO_TCP, TCP_NOPUSH, 0);

	*_sent = sent;
	return res;
}

int ffsig_ctl(ffsignal *t, fffd kq, const int *sigs, size_t nsigs, ffaio_handler handler)
{
	sigset_t mask;
	size_t i;
	fffd sigfd;

	if (handler == NULL) {
		if (t->fd != FF_BADFD) {
			close(t->fd);
		}
		ffkev_fin(t);
		return 0;
	}

	sigemptyset(&mask);
	for (i = 0;  i < nsigs;  i++) {
		sigaddset(&mask, sigs[i]);
	}

	sigfd = signalfd(FF_BADFD, &mask, SFD_NONBLOCK);
	if (sigfd == FF_BADFD)
		return -1;

	t->handler = handler;
	t->oneshot = 0;
	if (0 != ffkqu_attach(kq, sigfd, ffkev_ptr(t), FFKQU_ADD | FFKQU_READ)) {
		close(sigfd);
		return -1;
	}

	t->fd = sigfd;
	return 0;
}
#else
ffskt ffskt_create(uint domain, uint type, uint protocol)
{
	FF_ASSERT(0);
	return socket(domain, type, protocol);
}
#endif


static void _ffev_handler(void *udata)
{
	ffkevpost *p = udata;
	uint64 val;
	if (sizeof(uint64) != fffile_read(p->fd, &val, sizeof(uint64))) {
		FFDBG_PRINTLN(0, "fffile_read() error.  evfd: %d", p->fd);
		return;
	}
}

int ffkqu_post_attach(ffkevpost *p, fffd kq)
{
	ffkev_init(p);
	if (FF_BADFD == (p->fd = eventfd(0, EFD_NONBLOCK)))
		goto fail;

	p->handler = &_ffev_handler;
	p->udata = p;

	if (0 != ffkev_attach(p, kq, FFKQU_READ)) {
		fffile_close(p->fd);
		p->fd = FF_BADFD;
		goto fail;
	}

	return 0;

fail:
	return -1;
}

void ffkqu_post_detach(ffkevpost *p, fffd kq)
{
	fffile_close(p->fd);
	ffkev_fin(p);
}

int ffkqu_post(ffkevpost *p, void *data)
{
	uint64 val = 1;
	if (sizeof(uint64) != fffile_write(p->fd, &val, sizeof(uint64)))
		return -1;
	return 0;
}

int _ffaio_result(ffaio_task *t)
{
	int r = -1;

	if (t->canceled) {
		t->canceled = 0;
		fferr_set(ECANCELED);
		goto done;
	}

	if (t->ev->events & EPOLLERR) {
		int er = 0;
		(void)ffskt_getopt(t->sk, SOL_SOCKET, SO_ERROR, &er);
		fferr_set(er);
		goto done;
	}

	r = 0;

done:
	t->ev = NULL;
	return r;
}


/*
Asynchronous file I/O in Linux:

1. Setup:
 aioctx = io_setup()
 eventfd handle = eventfd()
 ffkqu_attach(eventfd handle) -> kq

2. Add task:
 io_submit() + eventfd handle -> aioctx

3. Process event:
 ffkqu_wait(kq) --(eventfd handle)-> _ffaio_fctxhandler()

4. Execute task:
 io_getevents(aioctx) --(ffkevent*)-> ffkev_call()
*/

static FFINL int io_setup(unsigned nr_events, aio_context_t *ctx_idp)
{
	return syscall(SYS_io_setup, nr_events, ctx_idp);
}

static FFINL int io_destroy(aio_context_t ctx_id)
{
	return syscall(SYS_io_destroy, ctx_id);
}

static FFINL int io_submit(aio_context_t ctx_id, long nr, struct iocb **iocbpp)
{
	return syscall(SYS_io_submit, ctx_id, nr, iocbpp);
}

static FFINL int io_getevents(aio_context_t ctx_id, long min_nr, long nr, struct io_event *events, struct timespec *timeout)
{
	return syscall(SYS_io_getevents, ctx_id, min_nr, nr, events, timeout);
}

typedef struct _ffaio_filectx {
	ffkevent kev;
	aio_context_t aioctx;
	fffd kq;
} _ffaio_filectx;

#define FFAIO_FCTX_N  16 // max aio file contexts
enum { _FFAIO_NWORKERS = 256 };
struct _ffaio_filectx_m {
	struct _ffaio_filectx **items;
	uint n;
	fflock lk;
};
static struct _ffaio_filectx_m _ffaio_fctx;

static int _ffaio_ctx_init(struct _ffaio_filectx *fx, fffd kq);
static struct _ffaio_filectx* _ffaio_ctx_get(fffd kq);
static void _ffaio_ctx_close(struct _ffaio_filectx *fx);
static void _ffaio_fctxhandler(void *udata);

int ffaio_fctxinit(void)
{
	if (NULL == (_ffaio_fctx.items = ffmem_allocT(FFAIO_FCTX_N, struct _ffaio_filectx*)))
		return 1;
	fflk_init(&_ffaio_fctx.lk);
	return 0;
}

void ffaio_fctxclose(void)
{
	uint n = _ffaio_fctx.n;
	for (uint i = 0;  i != n;  i++) {
		_ffaio_ctx_close(_ffaio_fctx.items[i]);
		ffmem_free(_ffaio_fctx.items[i]);
	}
	ffmem_free0(_ffaio_fctx.items);
	_ffaio_fctx.n = 0;
}

/** eventfd has signaled.  Call handlers of completed file I/O requests. */
static void _ffaio_fctxhandler(void *udata)
{
	_ffaio_filectx *fx = (_ffaio_filectx*)udata;
	uint64 ev_n;
	ssize_t i, r;
	struct io_event evs[64];
	struct timespec ts = {0, 0};

	r = fffile_read(fx->kev.fd, &ev_n, sizeof(uint64));
	if (r != sizeof(uint64)) {
#ifdef FFDBG_AIO
		ffdbg_print(0, "%s(): read error from eventfd: %L, errno: %d\n", FF_FUNC, r, errno);
#endif
		return;
	}

	while (ev_n != 0) {

		r = io_getevents(fx->aioctx, 1, FFCNT(evs), evs, &ts);
		if (r <= 0) {
#ifdef FFDBG_AIO
			if (r < 0)
				ffdbg_print(0, "%s(): io_getevents() error: %d\n", FF_FUNC, errno);
#endif
			return;
		}

		for (i = 0;  i < r;  i++) {

			ffkqu_entry e = {0};
			ffkevent *kev = (void*)(size_t)(evs[i].data & ~1);
			ffaio_filetask *ft = FF_GETPTR(ffaio_filetask, kev, kev);
			ft->result = evs[i].res;
			//const struct iocb *cb = (void*)evs[i].obj;

			e.data.ptr = (void*)(size_t)evs[i].data;
			ffkev_call(&e);
		}

		ev_n -= r;
	}
}

/** Find (or create new) file AIO context for kqueue descriptor.
Thread-safe. */
static struct _ffaio_filectx* _ffaio_ctx_get(fffd kq)
{
	struct _ffaio_filectx *fx, *fxnew = NULL;
	uint n = _ffaio_fctx.n;

	for (uint i = 0;  ;  i++) {

		if (i == FFAIO_FCTX_N) {
			errno = EINVAL;
			fx = NULL;
			break;
		}

		if (i == n) {

			if (fxnew == NULL) {
				if (NULL == (fxnew = ffmem_new(struct _ffaio_filectx)))
					return NULL;
				if (0 != _ffaio_ctx_init(fxnew, kq)) {
					ffmem_free(fxnew);
					return NULL;
				}
			}

			// atomically write new context pointer and increase counter
			fflk_lock(&_ffaio_fctx.lk);
			n = FF_READONCE(_ffaio_fctx.n);
			if (i != n) {
				// another thread has occupied this slot
				fflk_unlock(&_ffaio_fctx.lk);
				continue;
			}
			_ffaio_fctx.items[i] = fxnew;
			ffatom_fence_rel();
			FF_WRITEONCE(_ffaio_fctx.n, i + 1);
			fflk_unlock(&_ffaio_fctx.lk);

			return fxnew;
		}

		ffatom_fence_acq();
		fx = _ffaio_fctx.items[i];
		if (fx->kq == kq)
			break;
	}

	if (fxnew != NULL) {
		_ffaio_ctx_close(fxnew);
		ffmem_free(fxnew);
	}

	return fx;
}

static int _ffaio_ctx_init(struct _ffaio_filectx *fx, fffd kq)
{
	fx->aioctx = 0;
	ffkev_init(&fx->kev);

	if (0 != io_setup(_FFAIO_NWORKERS, &fx->aioctx))
		goto err;

	fx->kev.fd = eventfd(0, EFD_NONBLOCK);
	if (fx->kev.fd == FF_BADFD) {
		ffaio_fctxclose();
		goto err;
	}
	if (0 != ffkqu_attach(kq, fx->kev.fd, ffkev_ptr(&fx->kev), FFKQU_ADD | FFKQU_READ))
		goto err;
	fx->kq = kq;

	fx->kev.oneshot = 0;
	fx->kev.handler = &_ffaio_fctxhandler;
	ffatom_fence_rel();
	fx->kev.udata = fx;
	return 0;

err:
	_ffaio_ctx_close(fx);
	return 1;
}

static void _ffaio_ctx_close(struct _ffaio_filectx *fx)
{
	if (fx->kev.fd != FF_BADFD)
		fffile_close(fx->kev.fd);
	ffkev_fin(&fx->kev);

	if (fx->aioctx != 0)
		io_destroy(fx->aioctx);
}

int ffaio_fattach(ffaio_filetask *ft, fffd kq, uint direct)
{
	if (!direct) {
		//don't use AIO
		ft->cb.aio_resfd = FF_BADFD;
		return 0;
	}

	struct _ffaio_filectx *fx = _ffaio_ctx_get(kq);
	if (fx == NULL)
		return 1;

	ft->fctx = fx;
	ft->cb.aio_resfd = 0;
	return 0;
}

static ssize_t _ffaio_fop(ffaio_filetask *ft, void *data, size_t len, uint64 off, ffaio_handler handler, uint op)
{
	struct iocb *cb = &ft->cb;

	if (ft->kev.pending) {
		ft->kev.pending = 0;

		if (ft->result < 0) {
			errno = -ft->result;
			return -1;
		}

		return ft->result;
	}

	struct _ffaio_filectx *fx = ft->fctx;

	ffmem_tzero(cb);
	cb->aio_data = (uint64)(size_t)ffkev_ptr(&ft->kev);
	cb->aio_lio_opcode = op;
	cb->aio_flags = IOCB_FLAG_RESFD;
	cb->aio_resfd = fx->kev.fd;

	cb->aio_fildes = ft->kev.fd;
	cb->aio_buf = (uint64)(size_t)data;
	cb->aio_nbytes = len;
	cb->aio_offset = off;

	if (1 != io_submit(fx->aioctx, 1, &cb)) {
		if (errno == ENOSYS || errno == EAGAIN)
			return -3; //no resources for this I/O operation or AIO isn't supported
		return -1;
	}

	ft->kev.pending = 1;
	ft->kev.handler = handler;
	errno = EAGAIN;
	return -1;
}

ssize_t ffaio_fwrite(ffaio_filetask *ft, const void *data, size_t len, uint64 off, ffaio_handler handler)
{
	ssize_t r = -3;
	if ((int)ft->cb.aio_resfd != FF_BADFD)
		r = _ffaio_fop(ft, (void*)data, len, off, handler, IOCB_CMD_PWRITE);
	if (r == -3)
		r = fffile_pwrite(ft->kev.fd, data, len, off);
	return r;
}

ssize_t ffaio_fread(ffaio_filetask *ft, void *data, size_t len, uint64 off, ffaio_handler handler)
{
	ssize_t r = -3;
	if ((int)ft->cb.aio_resfd != FF_BADFD)
		r = _ffaio_fop(ft, data, len, off, handler, IOCB_CMD_PREAD);
	if (r == -3)
		r = fffile_pread(ft->kev.fd, data, len, off);
	return r;
}

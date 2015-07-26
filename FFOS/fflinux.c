/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/time.h>
#include <FFOS/error.h>
#include <FFOS/process.h>
#include <FFOS/sig.h>
#include <FFOS/timer.h>
#include <FFOS/socket.h>
#include <FFOS/asyncio.h>

#include <time.h>
#include <signal.h>
#include <sys/sendfile.h>
#include <sys/syscall.h>
#include <sys/eventfd.h>


int fferr_str(int code, char *dst, size_t dst_cap) {
	char *dst2;
	if (0 == dst_cap)
		return 0;
	dst2 = strerror_r(code, dst, dst_cap);
	if (dst2 != dst) {
		ssize_t len = ffmin(dst_cap - 1, strlen(dst2));
		memcpy(dst, dst2, len);
		dst[len] = '\0';
		return len;
	}
	return strlen(dst);
}


int fftmr_start(fftmr tmr, fffd kq, void *udata, int period_ms)
{
	struct itimerspec its;
	struct epoll_event e;
	unsigned neg = 0;

	e.events = EPOLLIN | EPOLLET;
	e.data.ptr = udata;
	if (0 != epoll_ctl(kq, EPOLL_CTL_ADD, tmr, &e))
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
		if (r != hsz)
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

enum { _FFAIO_NWORKERS = 256 };
static _ffaio_filectx _ffaio_fctx;

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
		ffdbg_print(0, "%s(): read error from eventfd: %L, errno: %d", FF_FUNC, r, errno);
#endif
		return;
	}

	while (ev_n != 0) {

		r = io_getevents(fx->aioctx, 1, FFCNT(evs), evs, &ts);
		if (r <= 0) {
#ifdef FFDBG_AIO
			if (r < 0)
				ffdbg_print(0, "%s(): io_getevents() error: %d", FF_FUNC, errno);
#endif
			return;
		}

		for (i = 0;  i < r;  i++) {

			ffkqu_entry e = {0};
			ffkevent *kev = (void*)(evs[i].data & ~1);
			ffaio_filetask *ft = FF_GETPTR(ffaio_filetask, kev, kev);
			ft->result = evs[i].res;
			//const struct iocb *cb = (void*)evs[i].obj;

			e.data.ptr = (void*)evs[i].data;
			ffkev_call(&e);
		}

		ev_n -= r;
	}
}

int ffaio_fctxinit(void)
{
	_ffaio_filectx *fx = &_ffaio_fctx;

	fx->aioctx = 0;
	fx->kq = FF_BADFD;
	ffkev_init(&fx->kev);

	if (0 != io_setup(_FFAIO_NWORKERS, &fx->aioctx))
		return 1;

	fx->kev.fd = eventfd(0, EFD_NONBLOCK);
	if (fx->kev.fd == FF_BADFD) {
		ffaio_fctxclose();
		return 1;
	}

	fx->kev.oneshot = 0;
	fx->kev.udata = fx;
	fx->kev.handler = &_ffaio_fctxhandler;
	return 0;
}

void ffaio_fctxclose(void)
{
	_ffaio_filectx *fx = &_ffaio_fctx;

	if (fx->kev.fd != FF_BADFD)
		fffile_close(fx->kev.fd);
	ffkev_fin(&fx->kev);

	if (fx->aioctx != 0) {
		io_destroy(fx->aioctx);
		fx->aioctx = 0;
	}
}

/* Attach eventfd to epoll */
int ffaio_fattach(ffaio_filetask *ft, fffd kq)
{
	_ffaio_filectx *fx = &_ffaio_fctx;

	if (fx->kq == FF_BADFD) {
		if (0 != ffkqu_attach(kq, fx->kev.fd, ffkev_ptr(&fx->kev), FFKQU_ADD | FFKQU_READ))
			return 1;
		fx->kq = kq;

	} else if (fx->kq != kq) {
		errno = EINVAL;
		return -1; //only 1 kq is supported
	}

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

	ffmem_tzero(cb);
	cb->aio_data = (uint64)ffkev_ptr(&ft->kev);
	cb->aio_lio_opcode = op;
	cb->aio_flags = IOCB_FLAG_RESFD;
	cb->aio_resfd = _ffaio_fctx.kev.fd;

	cb->aio_fildes = ft->kev.fd;
	cb->aio_buf = (uint64)data;
	cb->aio_nbytes = len;
	cb->aio_offset = off;

	if (1 != io_submit(_ffaio_fctx.aioctx, 1, &cb)) {
		if (errno == EAGAIN)
			return -3; //no resources for this I/O operation
		return -1;
	}

	ft->kev.pending = 1;
	ft->kev.handler = handler;
	errno = EAGAIN;
	return -1;
}

ssize_t ffaio_fwrite(ffaio_filetask *ft, const void *data, size_t len, uint64 off, ffaio_handler handler)
{
	ssize_t r = _ffaio_fop(ft, (void*)data, len, off, handler, IOCB_CMD_PWRITE);
	if (r == -3)
		r = fffile_pwrite(ft->kev.fd, data, len, off);
	return r;
}

ssize_t ffaio_fread(ffaio_filetask *ft, void *data, size_t len, uint64 off, ffaio_handler handler)
{
	ssize_t r = _ffaio_fop(ft, data, len, off, handler, IOCB_CMD_PREAD);
	if (r == -3)
		r = fffile_pread(ft->kev.fd, data, len, off);
	return r;
}

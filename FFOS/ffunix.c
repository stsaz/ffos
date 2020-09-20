/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/time.h>
#include <FFOS/file.h>
#include <FFOS/dir.h>
#include <FFOS/thread.h>
#include <FFOS/timer.h>
#include <FFOS/string.h>
#include <FFOS/process.h>
#include <FFOS/asyncio.h>
#include <FFOS/sig.h>

#include <sys/wait.h>
#if !defined FF_NOTHR && defined FF_BSD
#include <pthread_np.h>
#endif

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/resource.h>
#include <termios.h>
#include <string.h>


void fftime_now(fftime *t)
{
	struct timespec ts = {};
	(void)clock_gettime(CLOCK_REALTIME, &ts);
	fftime_fromtimespec(t, &ts);
}


#if !((defined FF_LINUX_MAINLINE || defined FF_BSD) && !defined FF_OLDLIBC)
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


int ffaio_recv(ffaio_task *t, ffaio_handler handler, void *d, size_t cap)
{
	ssize_t r;

	if (t->rpending) {
		t->rpending = 0;
		if (0 != _ffaio_result(t))
			return -1;
	}

	r = ffskt_recv(t->sk, d, cap, 0);
	if (!(r < 0 && fferr_again(fferr_last())))
		return r;

	t->rhandler = handler;
	t->rpending = 1;
	FFDBG_PRINTLN(FFDBG_KEV | 9, "task:%p, fd:%L, rhandler:%p, whandler:%p\n"
		, t, (size_t)t->fd, t->rhandler, t->whandler);
	return FFAIO_ASYNC;
}

int ffaio_send(ffaio_task *t, ffaio_handler handler, const void *d, size_t len)
{
	ssize_t r;

	if (t->wpending) {
		t->wpending = 0;
		if (0 != _ffaio_result(t))
			return -1;
	}

	r = ffskt_send(t->sk, d, len, 0);
	if (!(r < 0 && fferr_again(fferr_last())))
		return r;

	t->whandler = handler;
	t->wpending = 1;
	return FFAIO_ASYNC;
}

int ffaio_sendv(ffaio_task *t, ffaio_handler handler, ffiovec *iov, size_t iovcnt)
{
	ssize_t r;

	if (t->wpending) {
		t->wpending = 0;
		if (0 != _ffaio_result(t))
			return -1;
	}

	r = ffskt_sendv(t->sk, iov, iovcnt);
	if (!(r < 0 && fferr_again(fferr_last())))
		return r;

	t->whandler = handler;
	t->wpending = 1;
	return FFAIO_ASYNC;
}

ffskt ffaio_accept(ffaio_acceptor *acc, ffaddr *local, ffaddr *peer, int flags, ffkev_handler handler)
{
	ffskt sk;
	peer->len = FFADDR_MAXLEN;
	sk = ffskt_accept(acc->sk, &peer->a, &peer->len, flags);
	if (sk != FF_BADSKT)
		ffskt_local(sk, local);
	else if (fferr_again(errno))
		acc->handler = handler;
	return sk;
}

int ffaio_connect(ffaio_task *t, ffaio_handler handler, const struct sockaddr *addr, socklen_t addr_size)
{
	if (t->wpending) {
		t->wpending = 0;
		return _ffaio_result(t);
	}

	if (0 == ffskt_connect(t->fd, addr, addr_size))
		return 0;

	if (errno == EINPROGRESS) {
		t->whandler = handler;
		t->wpending = 1;
		return FFAIO_ASYNC;
	}

	return FFAIO_ERROR;
}

int ffaio_cancelasync(ffaio_task *t, int op, ffaio_handler oncancel)
{
	uint w = (t->whandler != NULL);
	ffaio_handler func;

	t->canceled = 1;

	if ((op & FFAIO_READ) && t->rhandler != NULL) {
		func = ((oncancel != NULL) ? oncancel : t->rhandler);
		t->rhandler = NULL;
		func(t->udata);
	}

	if ((op & FFAIO_WRITE) && w && t->whandler != NULL) {
		func = ((oncancel != NULL) ? oncancel : t->whandler);
		t->whandler = NULL;
		func(t->udata);
	}

	return 0;
}

#if !defined FF_LINUX

int _ffaio_result(ffaio_task *t)
{
	int r = -1;

	if (t->canceled) {
		t->canceled = 0;
		fferr_set(ECANCELED);
		goto done;
	}

	if (t->ev->flags & EV_ERROR) {
		fferr_set(t->ev->data);
		goto done;
	}
	if ((t->ev->filter == EVFILT_WRITE) && (t->ev->flags & EV_EOF)) {
		fferr_set(t->ev->fflags);
		goto done;
	}

	r = 0;

done:
	t->ev = NULL;
	return r;
}

#endif

ffskt ffaio_pipe_accept(ffkevent *kev, ffkev_handler handler)
{
	ffskt sk;
	if (FF_BADSKT == (sk = ffskt_accept(kev->sk, NULL, NULL, 0))) {
		if (fferr_again(errno))
			kev->handler = handler;
		return FF_BADSKT;
	}
	return sk;
}

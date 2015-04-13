/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/error.h>

#ifdef FF_LINUX
#include <linux/aio_abi.h>
#else
#include <aio.h>
#endif


#if defined FF_LINUX

/** Initialize file I/O context. */
FF_EXTN int ffaio_fctxinit(void);

FF_EXTN void ffaio_fctxclose(void);

struct ffaio_filetask {
	ffkevent kev;
	struct iocb cb;
	int result;
};

/** Attach ffaio_filetask to kqueue.
Linux: not thread-safe; only 1 kernel queue is supported for ALL AIO operations. */
FF_EXTN int ffaio_fattach(ffaio_filetask *ft, fffd kq);

#else //bsd:

#define ffaio_fctxinit()  (0)
#define ffaio_fctxclose()

struct ffaio_filetask {
	ffkevent kev;
	struct aiocb acb;
};

static FFINL int ffaio_fattach(ffaio_filetask *ft, fffd kq)
{
	ft->acb.aio_sigevent.sigev_notify_kqueue = kq;
	return 0;
}

#endif


/** Get AIO task result.
If 'canceled' flag is set, reset it and return -1. */
static FFINL int ffaio_result(ffaio_task *t) {
	if (t->evflags & FFKQU_ERR) {
		if (errno == ECANCELED)
			t->evflags &= ~FFKQU_ERR;
		return -1;
	}
	return 0;
}

typedef ffkevent ffaio_acceptor;

/** Initialize connection acceptor. */
static FFINL int ffaio_acceptinit(ffaio_acceptor *acc, fffd kq, ffskt lsk, void *udata, int family, int sktype)
{
	ffkev_init(acc);
	acc->sk = lsk;
	acc->udata = udata;
	(void)family;
	(void)sktype;
	return ffkqu_attach(kq, acc->sk, ffkev_ptr(acc), FFKQU_ADD | FFKQU_READ);
}

/** Close acceptor. */
#define ffaio_acceptfin(acc)

/** Begin async receive. */
static FFINL int ffaio_recv(ffaio_task *t, ffaio_handler handler, void *d, size_t cap) {
	(void)d;
	(void)cap;
	t->rhandler = handler;
	return FFAIO_ASYNC;
}

/** Begin async send. */
static FFINL ssize_t ffaio_send(ffaio_task *t, ffaio_handler handler, const void *d, size_t len) {
	(void)d;
	(void)len;
	t->whandler = handler;
	return FFAIO_ASYNC;
}

#define ffaio_sendv(aiotask, handler, iov, iovcnt) \
	ffaio_send(aiotask, handler, NULL, 0)

/** Get signaled events and flags of a kernel event structure. */
#define _ffaio_events(task, kqent)  ffkqu_result(kqent, (task)->fd)

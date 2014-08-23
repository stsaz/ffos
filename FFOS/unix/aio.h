/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/error.h>


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

typedef ffaio_task ffaio_acceptor;

/** Initialize connection acceptor. */
static FFINL int ffaio_acceptinit(ffaio_acceptor *acc, fffd kq, ffskt lsk, void *udata, int family, int sktype)
{
	ffaio_init(acc);
	acc->sk = lsk;
	acc->udata = udata;
	(void)family;
	(void)sktype;
	return ffaio_attach(acc, kq, FFKQU_READ);
}

/** Begin accepting. */
static FFINL int ffaio_acceptbegin(ffaio_acceptor *acc, ffaio_handler handler) {
	acc->rhandler = handler;
	return FFAIO_ASYNC;
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

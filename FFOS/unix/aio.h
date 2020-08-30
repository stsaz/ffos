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

/** Close all asynchronous file I/O contexts. */
FF_EXTN void ffaio_fctxclose(void);

struct ffaio_filetask {
	ffkevent kev;
	struct iocb cb;
	int result;
	void *fctx;
};

/** Attach ffaio_filetask to kqueue.
Linux: not thread-safe; only 1 kernel queue is supported for ALL AIO operations. */
FF_EXTN int ffaio_fattach(ffaio_filetask *ft, fffd kq, uint direct);

#elif defined FF_BSD

#define ffaio_fctxinit()  (0)
#define ffaio_fctxclose()

struct ffaio_filetask {
	ffkevent kev;
	struct aiocb acb;
};

static FFINL int ffaio_fattach(ffaio_filetask *ft, fffd kq, uint direct)
{
	(void)direct;
	ft->acb.aio_sigevent.sigev_notify_kqueue = kq;
	return 0;
}

#elif defined FF_APPLE

#define ffaio_fctxinit()  (0)
#define ffaio_fctxclose()

struct ffaio_filetask {
	ffkevent kev;
	struct aiocb acb;
};

static FFINL int ffaio_fattach(ffaio_filetask *ft, fffd kq, uint direct)
{
	(void)ft; (void)kq; (void)direct;
	return 0;
}

#endif


/** Get AIO task result.
If 'canceled' flag is set, reset it and return -1. */
FF_EXTN int _ffaio_result(ffaio_task *t);

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

/** Get signaled events and flags of a kernel event structure. */
#define _ffaio_events(task, kqent)  ffkqu_result(kqent)

/** Signals.
Copyright (c) 2013 Simon Zolin
*/

/*
ffsig_init
ffsig_ctl
ffsig_read
*/

#pragma once

#include <FFOS/signal.h>
#include <FFOS/asyncio.h>

#ifdef FF_WIN

#if defined FF_MSVC || defined FF_MINGW
enum {
	SIG_BLOCK,
	SIG_UNBLOCK,
};

enum {
	SIGINT = 1,
	SIGIO,
	SIGCHLD,
	SIGHUP,
	SIGUSR1,
};
#endif


enum FFSIGINFO_F {
	_FFSIGINFO_FDUMMY,

	//EXCEPTION_ACCESS_VIOLATION:
	// =0 //the thread attempted to read the inaccessible data
	// =1 //the thread attempted to write to an inaccessible address
	// =8 //the thread causes a user-mode data execution prevention (DEP) violation
};

typedef ffkevent ffsignal;

#define ffsig_init(sig)  ffkev_init(sig)

static FFINL int ffsig_mask(int how, const int *sigs, size_t nsigs)
{
	(void)how; (void)sigs; (void)nsigs;
	return 0;
}

typedef int ffsiginfo;

FF_EXTN int ffsig_read(ffsignal *sig, ffsiginfo *si);
FF_EXTN int ffsig_ctl(ffsignal *t, fffd kq, const int *sigs, size_t nsigs, ffaio_handler handler);

#else // UNIX:

typedef int ffsiginfo;

#ifdef FF_LINUX

typedef ffkevent ffsignal;
#define ffsig_init(sig)  ffkev_init(sig)

/** Get signal number
Return -1 on error */
static inline int ffsig_read(ffsignal *t, ffsiginfo *si)
{
	(void)si;
	return ffkqsig_read(t->fd, NULL);
}

#else // not Linux:

typedef ffaio_task ffsignal;
#define ffsig_init(sig)  ffaio_init(sig)

static inline int ffsig_read(ffsignal *t, ffsiginfo *si)
{
	(void)si;
	return ffkqsig_read(t->fd, t->ev);
}

#endif

/** If 'handler' is set, initialize kernel event for signals.
If 'handler' is NULL, remove event from the kernel.
Windows: Ctrl-events handling sequence: ctrl_handler() -> pipe -> kqueue -> sig_handler()
*/
static inline int ffsig_ctl(ffsignal *t, fffd kq, const int *sigs, size_t nsigs, ffaio_handler handler)
{
#ifdef FF_LINUX
	t->handler = handler;
	void *udata = ffkev_ptr(t);
#else
	t->rhandler = handler;
	void *udata = ffaio_kqudata(t);
#endif
	t->oneshot = 0;
	if (handler != NULL) {
		if (FFKQSIG_NULL == (t->fd = ffkqsig_attach(kq, sigs, nsigs, udata)))
			return -1;
	} else {
		if (0 != ffkqsig_detach(t->fd, kq))
			return -1;
#ifdef FF_LINUX
		ffkev_fin(t);
#else
		ffaio_fin(t);
#endif
	}
	return 0;
}

#endif

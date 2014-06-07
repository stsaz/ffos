/** Signals.
Copyright (c) 2013 Simon Zolin
*/

#pragma once

#include <FFOS/asyncio.h>

#if defined FF_LINUX
#include <sys/signalfd.h>
#endif

#ifdef FF_UNIX

/** Modify current signal mask. */
static FFINL int ffsig_mask(int how, const int *sigs, size_t nsigs) {
	sigset_t mask;
	size_t i;
	sigemptyset(&mask);
	for (i = 0;  i < nsigs;  i++) {
		sigaddset(&mask, sigs[i]);
	}
	return sigprocmask(how, &mask, NULL);
}
#endif

/** If 'handler' is set, initialize kernel event for signals.
If 'handler' is NULL, close signal fd.  If 'kq' and 'nsigs' are also specified, remove event from the kernel. */
FF_EXTN int ffsig_ctl(ffaio_task *t, fffd kq, const int *sigs, size_t nsigs, ffaio_handler handler);


#if defined FF_LINUX

/** Get signal number. */
static FFINL int ffsig_read(const ffaio_task *t) {
	struct signalfd_siginfo fdsi;
	ssize_t r = read(t->fd, &fdsi, sizeof(struct signalfd_siginfo));
	if (r != sizeof(struct signalfd_siginfo))
		return -1;
	return fdsi.ssi_signo;
}

#elif defined FF_BSD

static FFINL int ffsig_read(const ffaio_task *t) {
	int ident = t->ev->ident;
	t->ev->ident = -1;
	return ident;
}

#elif defined FF_WIN

FF_EXTN int ffsig_read(ffaio_task *t);

#endif

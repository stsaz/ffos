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

#else

#if defined FF_MSVC || defined FF_MINGW
enum {
	SIG_BLOCK
	, SIG_UNBLOCK
};
#endif

static FFINL int ffsig_mask(int how, const int *sigs, size_t nsigs) {
	return 0;
}

#endif

typedef ffkevent ffsignal;

#define ffsig_init  ffkev_init

/** If 'handler' is set, initialize kernel event for signals.
If 'handler' is NULL, remove event from the kernel. */
FF_EXTN int ffsig_ctl(ffsignal *sig, fffd kq, const int *sigs, size_t nsigs, ffaio_handler handler);

#if defined FF_LINUX

typedef struct signalfd_siginfo ffsiginfo;

#define ffsiginfo_fd(si)  ((si)->ssi_fd)

/** Get signal number.
@si: optional
Return -1 on error. */
static FFINL int ffsig_read(ffsignal *t, ffsiginfo *si)
{
	struct signalfd_siginfo fdsi;
	if (si == NULL)
		si = &fdsi;
	ssize_t r = read(t->fd, si, sizeof(struct signalfd_siginfo));
	if (r != sizeof(struct signalfd_siginfo)) {
		if (r != -1)
			errno = EAGAIN;
		return -1;
	}

	return si->ssi_signo;
}

#elif defined FF_BSD

typedef int ffsiginfo;

static FFINL int ffsig_read(ffsignal *t, ffsiginfo *si)
{
	int ident = t->ev->ident;
	if (t->ev->ident == -1) {
		errno = EAGAIN;
		return -1;
	}
	t->ev->ident = -1;
	return ident;
}

#elif defined FF_WIN

typedef int ffsiginfo;

FF_EXTN int ffsig_read(ffsignal *sig, ffsiginfo *si);

#endif

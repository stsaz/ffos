/**
Copyright (c) 2016 Simon Zolin
*/

#pragma once

#include <sys/signalfd.h>


typedef ffkevent ffsignal;

#define ffsig_init(sig)  ffkev_init(sig)

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

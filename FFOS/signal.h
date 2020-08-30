/** ffos: UNIX signals
2020, Simon Zolin
*/

/*
ffsig_mask
ffkqsig_attach
ffkqsig_detach
ffkqsig_read
*/

#pragma once

#include <FFOS/queue.h>

#if defined FF_LINUX

#include <sys/signalfd.h>
#include <sys/epoll.h>

typedef int ffkqsig;
#define FFKQSIG_NULL  (-1)

static inline ffkqsig ffkqsig_attach(ffkq kq, const int *sigs, ffuint nsigs, void *data)
{
	sigset_t mask;
	sigemptyset(&mask);
	for (ffuint i = 0;  i != nsigs;  i++) {
		sigaddset(&mask, sigs[i]);
	}

	ffkqsig sig = signalfd(-1, &mask, SFD_NONBLOCK);
	if (sig == -1)
		return -1;

	if (0 != ffkq_attach(kq, sig, data, FFKQ_READ)) {
		close(sig);
		return -1;
	}

	return sig;
}

static inline int ffkqsig_detach(ffkqsig sig, ffkq kq)
{
	(void)kq;
	return close(sig);
}

static inline int ffkqsig_read(ffkqsig sig, ffkq_event *ev)
{
	(void)ev;
	struct signalfd_siginfo info;
	ffssize r = read(sig, &info, sizeof(info));
	if (r != sizeof(info))
		return -1;
	return info.ssi_signo;
}

#else // not Linux:

#include <sys/event.h>

typedef int ffkqsig;
#define FFKQSIG_NULL  (-1)

static inline ffkqsig ffkqsig_attach(ffkq kq, const int *sigs, ffuint nsigs, void *data)
{
	struct kevent *evs = (struct kevent*)ffmem_alloc(nsigs * sizeof(struct kevent));
	if (evs == NULL)
		return -1;

	ffuint mask = 0;
	for (ffuint i = 0;  i != nsigs;  i++) {
		FF_ASSERT(sigs[i] < 32);
		EV_SET(&evs[i], sigs[i], EVFILT_SIGNAL, EV_ADD | EV_ENABLE, 0, 0, data);
		mask |= (1 << sigs[i]);
	}

	if (0 != kevent(kq, evs, nsigs, NULL, 0, NULL))
		mask = -1;

	ffmem_free(evs);
	return mask;
}

static inline int ffkqsig_detach(ffkqsig sig, ffkq kq)
{
	struct kevent *evs = (struct kevent*)ffmem_alloc(32 * sizeof(struct kevent));
	if (evs == NULL)
		return -1;

	ffuint mask = sig;
	ffuint i = 0;
	for (;;) {
		int r = ffbit_rfind32(mask) - 1;
		if (r < 0)
			break;
		mask &= ~(1 << r);
		EV_SET(&evs[i++], r, EVFILT_SIGNAL, EV_DELETE, 0, 0, NULL);
	}

	int r = 0;
	if (0 != kevent(kq, evs, i, NULL, 0, NULL))
		r = -1;

	ffmem_free(evs);
	return r;
}

static inline int ffkqsig_read(ffkqsig sig, ffkq_event *ev)
{
	(void)sig;
	int signo = ev->ident;
	ev->ident = -1;
	return signo;
}

#endif


/** Add/remove/set blocked signal mask for the current thread
how: SIG_BLOCK, SIG_UNBLOCK, SIG_SETMASK
Signal mask is inherited by clone/fork and preserved by execve() */
static inline void ffsig_mask(int how, const int *sigs, ffuint nsigs)
{
	sigset_t mask;
	sigemptyset(&mask);
	for (ffuint i = 0;  i != nsigs;  i++) {
		sigaddset(&mask, sigs[i]);
	}
	sigprocmask(how, &mask, NULL);
}

/** Attach signals receiver to kernel queue
sigs: signals to monitor
  Normally, they must be ignored (blocked) via ffsig_mask()
Return FFKQSIG_NULL on error */
static ffkqsig ffkqsig_attach(ffkq kq, const int *sigs, ffuint nsigs, void *data);

/** Detach signals receiver from kernel queue
Return !=0 on error */
static int ffkqsig_detach(ffkqsig sig, ffkq kq);

/** Read signal number after an event from kernel queue is received
User must call it in a loop until it returns an error
Return signal number
  <0 on error */
static int ffkqsig_read(ffkqsig sig, ffkq_event *ev);

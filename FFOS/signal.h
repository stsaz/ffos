/** ffos: UNIX signals
2020, Simon Zolin
*/

/*
ffsig_mask
ffsig_subscribe
ffsig_raise
ffkqsig_attach
ffkqsig_detach
ffkqsig_read ffkqsig_readinfo
*/

#pragma once

#include <FFOS/base.h>

struct ffsig_info {
	ffuint sig; // enum FFSIG
	void *addr; // FFSIG_SEGV: the virtual address of the inaccessible data
	ffuint flags; // enum FFSIGINFO_F
	void *si; // OS-specific information object
	int pid; // Linux: sender's PID
};

typedef void (*ffsig_handler)(struct ffsig_info *i);

#ifdef FF_WIN

enum FFSIG {
	FFSIG_INT = CONTROL_C_EXIT,
	FFSIG_SEGV = EXCEPTION_ACCESS_VIOLATION, // The thread tried to read from or write to a virtual address for which it does not have the appropriate access
	FFSIG_ABORT = EXCEPTION_BREAKPOINT, // A breakpoint was encountered
	FFSIG_ILL = EXCEPTION_ILLEGAL_INSTRUCTION, // The thread tried to execute an invalid instruction
	FFSIG_STACK = EXCEPTION_STACK_OVERFLOW, // The thread used up its stack
	FFSIG_FPE = EXCEPTION_FLT_DIVIDE_BY_ZERO,
};

/** User's signal handler */
FF_EXTERN ffsig_handler _ffsig_userhandler;

/** Called by OS when a CTRL event is received from console */
static BOOL WINAPI _ffsig_ctrl_handler(DWORD ctrl)
{
	if (ctrl == CTRL_C_EVENT) {
		struct ffsig_info i = {};
		i.sig = FFSIG_INT;
		_ffsig_userhandler(&i);
		return 1;
	}
	return 0;
}

/** Called by OS on a program exception
We reset to the default exception handler on entry */
static LONG WINAPI _ffsig_exc_handler(struct _EXCEPTION_POINTERS *inf)
{
	SetUnhandledExceptionFilter(NULL);

	struct ffsig_info i = {};
	i.sig = inf->ExceptionRecord->ExceptionCode; //EXCEPTION_*
	i.si = inf;

	switch (inf->ExceptionRecord->ExceptionCode) {
	case EXCEPTION_ACCESS_VIOLATION:
		i.flags = inf->ExceptionRecord->ExceptionInformation[0];
		i.addr = (void*)inf->ExceptionRecord->ExceptionInformation[1];
		break;
	}

	_ffsig_userhandler(&i);
	return EXCEPTION_CONTINUE_SEARCH;
}

static inline int ffsig_subscribe(ffsig_handler handler, const ffuint *sigs, ffuint nsigs)
{
	if (handler != NULL)
		_ffsig_userhandler = handler;

	ffbool exc = 0;
	for (ffuint i = 0;  i != nsigs;  i++) {
		if (sigs[i] == (ffuint)FFSIG_INT)
			SetConsoleCtrlHandler(_ffsig_ctrl_handler, 1);
		else
			exc = 1;
	}

	if (exc)
		SetUnhandledExceptionFilter(&_ffsig_exc_handler);
	return 0;
}

#else

#include <FFOS/queue.h>
#include <signal.h>

enum FFSIG {
	FFSIG_INT = SIGINT,
	FFSIG_SEGV = SIGSEGV, // Invalid memory reference
	FFSIG_ABORT = SIGABRT, // Abort signal
	FFSIG_FPE = SIGFPE, // Floating-point exception
	FFSIG_ILL = SIGILL, // Illegal Instruction
	FFSIG_STACK = 0x40000000 | SIGSEGV, // Stack overflow
};

#if defined FF_LINUX

#include <sys/signalfd.h>
#include <sys/epoll.h>
#include <signal.h>

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

static inline int ffkqsig_readinfo(ffkqsig sig, ffkq_event *ev, struct ffsig_info *info)
{
	(void)ev;
	struct signalfd_siginfo si;
	ffssize r = read(sig, &si, sizeof(si));
	if (r != sizeof(si))
		return -1;

	if (info != NULL) {
		info->pid = si.ssi_pid;
	}
	return si.ssi_signo;
}

static inline int ffkqsig_read(ffkqsig sig, ffkq_event *ev)
{
	return ffkqsig_readinfo(sig, ev, NULL);
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
	if (sig == FFKQSIG_NULL) {
		errno = EBADFD;
		return -1;
	}

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

#endif // #if defined FF_LINUX

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

/** User's signal handler */
FF_EXTERN ffsig_handler _ffsig_userhandler;

/** Called by OS with a signal we subscribed to
If user set FFSIG_STACK handler, then for SIGSEGV this function is called on alternative stack
Prior to this call the signal handler is set to default (SA_RESETHAND) */
static void _ffsig_exc_handler(int signo, siginfo_t *info, void *ucontext)
{
	(void)ucontext;
	struct ffsig_info i = {
		.sig = (ffuint)signo,
		.addr = info->si_addr,
		.flags = (ffuint)info->si_code,
		.si = info,
	};
	_ffsig_userhandler(&i);
}

static inline int ffsig_subscribe(ffsig_handler handler, const ffuint *sigs, ffuint nsigs)
{
	if (handler != NULL)
		_ffsig_userhandler = handler;

	int have_stack_sig = 0;
	struct sigaction sa = {};
	sa.sa_sigaction = &_ffsig_exc_handler;

	for (ffuint i = 0;  i != nsigs;  i++) {
		if (sigs[i] != FFSIG_STACK)
			continue;

		sa.sa_flags = SA_SIGINFO | SA_RESETHAND | SA_ONSTACK;
		stack_t stack;
		stack.ss_sp = ffmem_alloc(SIGSTKSZ);
		stack.ss_size = SIGSTKSZ;
		stack.ss_flags = 0;
		if (stack.ss_sp == NULL)
			return -1;
		if (0 != sigaltstack(&stack, NULL))
			return -1;
		if (0 != sigaction(SIGSEGV, &sa, NULL))
			return -1;
		have_stack_sig = 1;
		break;
	}

	sa.sa_flags = SA_SIGINFO | SA_RESETHAND;
	for (ffuint i = 0;  i != nsigs;  i++) {
		if (sigs[i] == FFSIG_STACK
			|| (sigs[i] == FFSIG_SEGV && have_stack_sig))
			continue;

		if (0 != sigaction(sigs[i], &sa, NULL))
			return -1;
	}
	return 0;
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
static int ffkqsig_readinfo(ffkqsig sig, ffkq_event *ev, struct ffsig_info *info);

#endif


/** Subscribe to system signals
handler: signal handler; only one per process
  NULL: use the current handler
Return 0 on success */
static int ffsig_subscribe(ffsig_handler handler, const ffuint *sigs, ffuint nsigs);

/** Raise the specified signal */
static inline void ffsig_raise(int sig)
{
	switch (sig) {
	case FFSIG_ABORT:
		abort();
		break;

	case FFSIG_STACK:
		ffsig_raise(sig);
		break;

	case FFSIG_SEGV: {
		void *addr = (void*)0x16;
		*(int*)addr = -1;
		break;
	}

	case FFSIG_FPE: {
		int i = 0;
		i = 10 / i;
		break;
	}
	}
}

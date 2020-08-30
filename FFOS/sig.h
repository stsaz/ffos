/** Signals.
Copyright (c) 2013 Simon Zolin
*/

/*
ffsig_init
ffsig_ctl
ffsig_read

ffsig_subscribe
ffsig_raise
*/

#pragma once

#include <FFOS/asyncio.h>

struct ffsig_info {
	ffuint sig; // enum FFSIG
	void *addr; // FFSIG_SEGV: the virtual address of the inaccessible data
	ffuint flags; // enum FFSIGINFO_F
	void *si; // OS-specific information object
};

typedef void (*ffsig_handler)(struct ffsig_info *i);

#ifdef FF_WIN

#if defined FF_MSVC || defined FF_MINGW
enum {
	SIG_BLOCK,
	SIG_UNBLOCK,
};

enum {
	SIGINT = 1,
	SIGIO,
	SIGHUP,
	SIGUSR1,
};
#endif

enum FFSIG {
	FFSIG_SEGV = EXCEPTION_ACCESS_VIOLATION, //The thread tried to read from or write to a virtual address for which it does not have the appropriate access
	FFSIG_ABORT = EXCEPTION_BREAKPOINT, //A breakpoint was encountered
	FFSIG_ILL = EXCEPTION_ILLEGAL_INSTRUCTION, //The thread tried to execute an invalid instruction
	FFSIG_STACK = EXCEPTION_STACK_OVERFLOW, //The thread used up its stack
	FFSIG_FPE = EXCEPTION_FLT_DIVIDE_BY_ZERO,
};

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

/** User's signal handler */
FF_EXTN ffsig_handler _ffsig_userhandler;

/** Called by OS on a program exception
We reset to the default exception handler on entry */
static LONG WINAPI _ffsig_func(struct _EXCEPTION_POINTERS *inf)
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
	(void)sigs; (void)nsigs;
	if (handler != NULL)
		_ffsig_userhandler = handler;

	SetUnhandledExceptionFilter(&_ffsig_func);
	return 0;
}

#else // UNIX:

#include <FFOS/signal.h>

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


enum FFSIG {
	FFSIG_SEGV = SIGSEGV, // Invalid memory reference
	FFSIG_ABORT = SIGABRT, // Abort signal
	FFSIG_FPE = SIGFPE, // Floating-point exception
	FFSIG_ILL = SIGILL, // Illegal Instruction
	FFSIG_STACK = 0x80000000 | SIGSEGV, // Stack overflow
};

/** User's signal handler */
FF_EXTN ffsig_handler _ffsig_userhandler;

/** Called by OS with a signal we subscribed to
If user set FFSIG_STACK handler, then for SIGSEGV this function is called on alternative stack
Prior to this call the signal handler is set to default (SA_RESETHAND) */
static void _ffsig_func(int signo, siginfo_t *info, void *ucontext)
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
	sa.sa_sigaction = &_ffsig_func;

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

#endif

/** Subscribe to system signals
handler: Set signal handler.  Only one handler per process can be used.
  NULL: use the current handler
Return 0 on success */
static int ffsig_subscribe(ffsig_handler handler, const ffuint *sigs, ffuint nsigs);

/** Raise the specified signal */
static inline void ffsig_raise(ffuint sig)
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

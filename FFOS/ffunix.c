/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/time.h>
#include <FFOS/file.h>
#include <FFOS/dir.h>
#include <FFOS/thread.h>
#include <FFOS/timer.h>
#include <FFOS/string.h>
#include <FFOS/process.h>
#include <FFOS/asyncio.h>
#include <FFOS/sig.h>

#include <sys/wait.h>
#if !defined FF_NOTHR && defined FF_BSD
#include <pthread_np.h>
#endif

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/resource.h>
#include <termios.h>
#include <string.h>


fffd ffpipe_create_named(const char *name, uint flags)
{
	ffskt p;
	struct sockaddr_un a = {0};
#if defined FF_BSD || defined FF_APPLE
	a.sun_len = sizeof(struct sockaddr_un);
#endif
	a.sun_family = AF_UNIX;
	size_t len = strlen(name);
	if (len + 1 > sizeof(a.sun_path))
		return FF_BADFD;
	strcpy(a.sun_path, name);

	if (FF_BADFD == (p = ffskt_create(AF_UNIX, SOCK_STREAM | (flags & SOCK_NONBLOCK), 0)))
		return FF_BADFD;

	if (0 != bind(p, (void*)&a, sizeof(struct sockaddr_un)))
		goto err;

	if (0 != ffskt_listen(p, 0))
		goto err;

	return p;

err:
	ffskt_close(p);
	return FF_BADFD;
}

fffd ffpipe_connect(const char *name)
{
	ffskt p;
	struct sockaddr_un a = {0};
#if defined FF_BSD || defined FF_APPLE
	a.sun_len = sizeof(struct sockaddr_un);
#endif
	a.sun_family = AF_UNIX;
	size_t len = strlen(name);
	if (len + 1 > sizeof(a.sun_path))
		return FF_BADFD;
	strcpy(a.sun_path, name);

	if (FF_BADSKT == (p = ffskt_create(AF_UNIX, SOCK_STREAM, 0)))
		return FF_BADFD;

	if (0 != ffskt_connect(p, (void*)&a, sizeof(struct sockaddr_un))) {
		ffskt_close(p);
		return FF_BADFD;
	}

	return p;
}


void fftime_now(fftime *t)
{
	struct timespec ts = { 0 };
	(void)clock_gettime(CLOCK_REALTIME, &ts);
	fftime_fromtimespec(t, &ts);
}


ffps ffps_exec2(const char *filename, ffps_execinfo *info)
{
	pid_t p = vfork();
	if (p != 0)
		return p;

	struct sigaction sa = {};
	sa.sa_handler = SIG_DFL;
	sigaction(SIGPIPE, &sa, NULL);

	if (info->in != FF_BADFD)
		dup2(info->in, ffstdin);
	if (info->out != FF_BADFD)
		dup2(info->out, ffstdout);
	if (info->err != FF_BADFD)
		dup2(info->err, ffstderr);
	execve(filename, (char**)info->argv, (char**)info->env);
	ffps_exit(255);
	return 0;
}

int ffps_wait(ffps h, uint timeout, int *exit_code)
{
	siginfo_t s;
	int r, f = 0;

	if (timeout != (uint)-1) {
		f = WNOHANG;
		s.si_pid = 0;
	}

	r = waitid(P_PID, (id_t)h, &s, WEXITED | f);
	if (r != 0)
		return -1;

	if (s.si_pid == 0) {
		errno = ETIMEDOUT;
		return -1;
	}

	if (exit_code != NULL) {
		if (s.si_code == CLD_EXITED)
			*exit_code = s.si_status;
		else
			*exit_code = -s.si_status;
	}
	return 0;
}

ffps ffps_createself_bg(const char *arg)
{
	ffps ps = ffps_fork();
	if (ps == 0) {
		setsid();
		umask(0);

		fffd f;
		if (FF_BADFD == (f = fffile_open("/dev/null", O_RDWR))) {
			FFDBG_PRINTLN(1, "error: %s: %s", fffile_open_S, "/dev/null");
			return FF_BADFD;
		}
		dup2(f, ffstdin);
		dup2(f, ffstdout);
		dup2(f, ffstderr);
		if (f > ffstderr)
			fffile_close(f);
	}
	return ps;
}

/* Note: $PATH should be used to find a file in case argv0 is without path, e.g. "binary". */
const char* _ffpath_real(char *name, size_t cap, const char *argv0)
{
	char real[PATH_MAX];
	if (NULL == realpath(argv0, real))
		return NULL;
	uint n = strlen(real) + 1;
	if (n > cap)
		return NULL;
	memcpy(name, real, n);
	return name;
}


/** User's signal handler. */
static ffsig_handler _ffsig_userhandler;

/** Called by OS with a signal we subscribed to.
If user set FFSIG_STACK handler, then for SIGSEGV this function is called on alternative stack.
Prior to this call the signal handler is set to default (SA_RESETHAND). */
static void _ffsigfunc(int signo, siginfo_t *info, void *ucontext)
{
	struct ffsig_info i = {
		.sig = signo,
		.addr = info->si_addr,
		.flags = info->si_code,
		.si = info,
	};
	_ffsig_userhandler(&i);
}

int ffsig_subscribe(ffsig_handler handler, const uint *sigs, uint nsigs)
{
	if (handler != NULL)
		_ffsig_userhandler = handler;

	int have_stack_sig = 0;
	struct sigaction sa = {};
	sa.sa_sigaction = &_ffsigfunc;

	for (uint i = 0;  i != nsigs;  i++) {
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
	for (uint i = 0;  i != nsigs;  i++) {
		if (sigs[i] == FFSIG_STACK
			|| (sigs[i] == FFSIG_SEGV && have_stack_sig))
			continue;

		if (0 != sigaction(sigs[i], &sa, NULL))
			return -1;
	}
	return 0;
}


#if !((defined FF_LINUX_MAINLINE || defined FF_BSD) && !defined FF_OLDLIBC)
ffskt ffskt_accept(ffskt listenSk, struct sockaddr *a, socklen_t *addrSize, int flags)
{
	ffskt sk = accept(listenSk, a, addrSize);
	if ((flags & SOCK_NONBLOCK) && sk != FF_BADSKT && 0 != ffskt_nblock(sk, 1)) {
		ffskt_close(sk);
		return FF_BADSKT;
	}
	return sk;
}
#endif


int ffaio_recv(ffaio_task *t, ffaio_handler handler, void *d, size_t cap)
{
	ssize_t r;

	if (t->rpending) {
		t->rpending = 0;
		if (0 != _ffaio_result(t))
			return -1;
	}

	r = ffskt_recv(t->sk, d, cap, 0);
	if (!(r < 0 && fferr_again(fferr_last())))
		return r;

	t->rhandler = handler;
	t->rpending = 1;
	FFDBG_PRINTLN(FFDBG_KEV | 9, "task:%p, fd:%L, rhandler:%p, whandler:%p\n"
		, t, (size_t)t->fd, t->rhandler, t->whandler);
	return FFAIO_ASYNC;
}

int ffaio_send(ffaio_task *t, ffaio_handler handler, const void *d, size_t len)
{
	ssize_t r;

	if (t->wpending) {
		t->wpending = 0;
		if (0 != _ffaio_result(t))
			return -1;
	}

	r = ffskt_send(t->sk, d, len, 0);
	if (!(r < 0 && fferr_again(fferr_last())))
		return r;

	t->whandler = handler;
	t->wpending = 1;
	return FFAIO_ASYNC;
}

int ffaio_sendv(ffaio_task *t, ffaio_handler handler, ffiovec *iov, size_t iovcnt)
{
	ssize_t r;

	if (t->wpending) {
		t->wpending = 0;
		if (0 != _ffaio_result(t))
			return -1;
	}

	r = ffskt_sendv(t->sk, iov, iovcnt);
	if (!(r < 0 && fferr_again(fferr_last())))
		return r;

	t->whandler = handler;
	t->wpending = 1;
	return FFAIO_ASYNC;
}

ffskt ffaio_accept(ffaio_acceptor *acc, ffaddr *local, ffaddr *peer, int flags, ffkev_handler handler)
{
	ffskt sk;
	peer->len = FFADDR_MAXLEN;
	sk = ffskt_accept(acc->sk, &peer->a, &peer->len, flags);
	if (sk != FF_BADSKT)
		ffskt_local(sk, local);
	else if (fferr_again(errno))
		acc->handler = handler;
	return sk;
}

int ffaio_connect(ffaio_task *t, ffaio_handler handler, const struct sockaddr *addr, socklen_t addr_size)
{
	if (t->wpending) {
		t->wpending = 0;
		return _ffaio_result(t);
	}

	if (0 == ffskt_connect(t->fd, addr, addr_size))
		return 0;

	if (errno == EINPROGRESS) {
		t->whandler = handler;
		t->wpending = 1;
		return FFAIO_ASYNC;
	}

	return FFAIO_ERROR;
}

int ffaio_cancelasync(ffaio_task *t, int op, ffaio_handler oncancel)
{
	uint w = (t->whandler != NULL);
	ffaio_handler func;

	t->canceled = 1;

	if ((op & FFAIO_READ) && t->rhandler != NULL) {
		func = ((oncancel != NULL) ? oncancel : t->rhandler);
		t->rhandler = NULL;
		func(t->udata);
	}

	if ((op & FFAIO_WRITE) && w && t->whandler != NULL) {
		func = ((oncancel != NULL) ? oncancel : t->whandler);
		t->whandler = NULL;
		func(t->udata);
	}

	return 0;
}

#if !defined FF_LINUX

int _ffaio_result(ffaio_task *t)
{
	int r = -1;

	if (t->canceled) {
		t->canceled = 0;
		fferr_set(ECANCELED);
		goto done;
	}

	if (t->ev->flags & EV_ERROR) {
		fferr_set(t->ev->data);
		goto done;
	}
	if ((t->ev->filter == EVFILT_WRITE) && (t->ev->flags & EV_EOF)) {
		fferr_set(t->ev->fflags);
		goto done;
	}

	r = 0;

done:
	t->ev = NULL;
	return r;
}

#endif

ffskt ffaio_pipe_accept(ffkevent *kev, ffkev_handler handler)
{
	ffskt sk;
	if (FF_BADSKT == (sk = ffskt_accept(kev->sk, NULL, NULL, 0))) {
		if (fferr_again(errno))
			kev->handler = handler;
		return FF_BADSKT;
	}
	return sk;
}

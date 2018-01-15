/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/time.h>
#include <FFOS/file.h>
#include <FFOS/dir.h>
#include <FFOS/thread.h>
#include <FFOS/string.h>
#include <FFOS/process.h>
#include <FFOS/asyncio.h>

#include <sys/wait.h>
#if !defined FF_NOTHR && defined FF_BSD
#include <pthread_np.h>
#endif

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/resource.h>
#include <termios.h>
#include <string.h>


int ffstd_attr(fffd fd, uint attr, uint val)
{
	struct termios t;
	if (0 != tcgetattr(fd, &t))
		return -1;

	if (attr & FFSTD_ECHO) {
		if (val & FFSTD_ECHO)
			t.c_lflag |= ECHO;
		else
			t.c_lflag &= ~ECHO;
	}

	if (attr & FFSTD_LINEINPUT) {
		if (val & FFSTD_LINEINPUT)
			t.c_lflag |= ICANON;
		else {
			t.c_lflag &= ~ICANON;
			t.c_cc[VTIME] = 0;
			t.c_cc[VMIN] = 1;
		}
	}

	tcsetattr(fd, TCSANOW, &t);
	return 0;
}

// ESC [ 1;N
static const uint _ffstd_key_esc1[] = {
	FFKEY_SHIFT /*N == 2*/, FFKEY_ALT, FFKEY_SHIFT | FFKEY_ALT,
	FFKEY_CTRL, FFKEY_CTRL | FFKEY_SHIFT, FFKEY_CTRL | FFKEY_ALT, FFKEY_CTRL | FFKEY_SHIFT | FFKEY_ALT,
};

/** Parse key received from terminal. */
int ffstd_key(const char *data, size_t *len)
{
	int r = 0;
	const char *d = data;
	if (*len == 0)
		return -1;

	if (d[0] == '\x1b' && *len >= 3 && d[1] == '[') {

		if (d[2] == '1' && *len > 5 && d[3] == ';') {
			uint m = (byte)d[4];
			if (m - '2' >= FFCNT(_ffstd_key_esc1))
				return -1;
			r = _ffstd_key_esc1[m - '2'];
			d += FFSLEN("1;N");
		}
		d += 2;

		if (*d >= 'A' && *d <= 'D') {
			*len = d - data + 1;
			r |= FFKEY_UP + *d - 'A';
			return r;
		}

		return -1;
	}

	*len = 1;
	return d[0];
}

int ffstd_event(fffd fd, ffstd_ev *ev)
{
	ssize_t r = fffile_read(fd, ev->buf, sizeof(ev->buf));
	if (r < 0 && fferr_again(fferr_last()))
		return 0;
	else if (r == 0) {
		errno = EINVAL;
		return -1;
	}
	ev->data = ev->buf;
	ev->datalen = r;
	return 1;
}


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


static const ffsyschar * fullPath(ffdirentry *ent, ffsyschar *nm, size_t nmlen) {
	if (ent->pathlen + nmlen + FFSLEN("/") >= ent->pathcap) {
		errno = EOVERFLOW;
		return NULL;
	}
	ent->path[ent->pathlen] = FFPATH_SLASH;
	memcpy(ent->path + ent->pathlen + 1, nm, nmlen);
	ent->path[ent->pathlen + nmlen + 1] = '\0';
	return ent->path;
}

ffdir_einfo * ffdir_entryinfo(ffdirentry *ent)
{
	int r;
	const ffsyschar *nm = fullPath(ent, ffdir_entryname(ent), ent->namelen);
	if (nm == NULL)
		return NULL;
	r = fffile_infofn(nm, &ent->info);
	ent->path[ent->pathlen] = '\0';
	return r == 0 ? &ent->info : NULL;
}


void fftime_now(fftime *t)
{
	struct timespec ts = { 0 };
	(void)clock_gettime(CLOCK_REALTIME, &ts);
	fftime_fromtimespec(t, &ts);
}


void ffthd_sleep(uint ms)
{
	struct timespec ts;

	if (ms == (uint)-1) {
		ts.tv_sec = 0x7fffffff;
		ts.tv_nsec = 0;
	}
	else {
		ts.tv_sec = ms / 1000;
		ts.tv_nsec = (ms % 1000) * 1000 * 1000;
	}

	while (0 != nanosleep(&ts, &ts) && errno == EINTR) {
	}
}

ffps ffps_exec(const char *filename, const char **argv, const char **env)
{
	pid_t p = vfork();
	if (p == 0) {
		execve(filename, (char**)argv, (char**)env);
		ffps_exit(255);
		return 0;
	}
	return p;
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

static int _ffps_perf(int who, struct ffps_perf *p, uint flags)
{
	int rc = 0, r;

	if (flags & FFPS_PERF_REALTIME)
		fftime_now(&p->realtime);

	if (flags & FFPS_PERF_RUSAGE) {
		struct rusage u;
		if (0 == (r = getrusage(who, &u))) {
			fftime_fromtimeval(&p->usertime, &u.ru_utime);
			fftime_fromtimeval(&p->systime, &u.ru_stime);
			p->pagefaults = u.ru_minflt + u.ru_majflt;
			p->maxrss = u.ru_maxrss;
			p->inblock = u.ru_inblock;
			p->outblock = u.ru_oublock;
			p->vctxsw = u.ru_nvcsw;
			p->ivctxsw = u.ru_nivcsw;
		}
		rc |= r;
	}

	if (flags & FFPS_PERF_CPUTIME) {
		uint c = (who == RUSAGE_SELF) ? CLOCK_PROCESS_CPUTIME_ID : CLOCK_THREAD_CPUTIME_ID;
		struct timespec ts;
		if (0 == (r = clock_gettime(c, &ts)))
			fftime_fromtimespec(&p->cputime, &ts);
		rc |= r;
	}

	return rc;
}

int ffps_perf(struct ffps_perf *p, uint flags)
{
	return _ffps_perf(RUSAGE_SELF, p, flags);
}

int ffthd_perf(struct ffps_perf *p, uint flags)
{
	return _ffps_perf(RUSAGE_THREAD, p, flags);
}


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

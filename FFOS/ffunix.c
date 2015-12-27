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

#include <termios.h>
#include <string.h>


void ffstd_echo(fffd fd, uint enable)
{
	struct termios t;
	if (0 != tcgetattr(fd, &t))
		return;

	if (enable)
		t.c_lflag |= ECHO;
	else
		t.c_lflag &= ~ECHO;
	tcsetattr(fd, TCSANOW, &t);
}

void ffstd_keypress(fffd fd, uint enable)
{
	struct termios t;
	if (0 != tcgetattr(fd, &t))
		return;

	if (enable) {
		t.c_lflag &= ~ICANON;
		t.c_cc[VTIME] = 0;
		t.c_cc[VMIN] = 1;
	} else
		t.c_lflag |= ICANON;
	tcsetattr(0, TCSANOW, &t);
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

void fftime_split(ffdtm *dt, const fftime *_t, enum FF_TIMEZONE tz)
{
	const time_t t = _t->s;
	struct tm tt;
	if (tz == FFTIME_TZUTC)
		gmtime_r(&t, &tt);
	else
		localtime_r(&t, &tt);
	fftime_fromtm(dt, &tt);
	dt->msec = _t->mcs / 1000;
}

fftime * fftime_join(fftime *t, const ffdtm *dt, enum FF_TIMEZONE tz)
{
	struct tm tt;

	if (tz == FFTIME_TZNODATE) {
		t->s = dt->hour * 24 * 60 + dt->min * 60 + dt->sec;
		t->mcs = dt->msec * 1000;
		return t;
	}

	FF_ASSERT(dt->year >= 1970);
	fftime_totm(&tt, dt);
	if (tz == FFTIME_TZUTC)
		t->s = timegm(&tt);
	else
		t->s = mktime(&tt);
	t->mcs = dt->msec * 1000;
	return t;
}


#if !defined FF_NOTHR
ffthd ffthd_create(ffthdproc proc, void *param, size_t stack_size)
{
	ffthd th = 0;
	typedef void* (*start_routine) (void *);
	pthread_attr_t attr;
	pthread_attr_t *pattr = NULL;

	if (stack_size != 0) {
		if (0 != pthread_attr_init(&attr))
			return 0;
		pattr = &attr;
		if (0 != pthread_attr_setstacksize(&attr, stack_size))
			goto end;
	}

	if (0 != pthread_create(&th, pattr, (start_routine)proc, param))
		th = 0;

end:
	if (pattr != NULL)
		pthread_attr_destroy(pattr);

	return th;
}

int ffthd_join(ffthd th, uint timeout_ms, int *exit_code)
{
	void *result;
	int r;

	if (timeout_ms == (uint)-1) {
		r = pthread_join(th, &result);
	}

#if defined FF_LINUX
	else if (timeout_ms == 0) {
		r = pthread_tryjoin_np(th, &result);
		if (r == EBUSY)
			r = ETIMEDOUT;
	}
#endif

	else {
		struct timespec ts;

		(void)clock_gettime(CLOCK_REALTIME, &ts);
		ts.tv_sec += timeout_ms / 1000;
		ts.tv_nsec += (timeout_ms % 1000) * 1000 * 1000;
		if (ts.tv_nsec >= 1000 * 1000 * 1000) {
			ts.tv_sec += 1;
			ts.tv_nsec -= 1000 * 1000 * 1000;
		}

		r = pthread_timedjoin_np(th, &result, &ts);
	}

	if (r != 0)
		return r;

	if (exit_code != NULL)
		*exit_code = (int)(size_t)result;
	return 0;
}
#endif

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

fffd ffps_exec(const char *filename, const char **argv, const char **env)
{
	pid_t p = vfork();
	if (p == 0) {
		execve(filename, (char**)argv, (char**)env);
		ffps_exit(255);
		return 0;
	}
	return p;
}

int ffps_wait(fffd h, uint timeout, int *exit_code)
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
	if (0 == ffskt_connect(t->fd, addr, addr_size))
		return FFAIO_OK;

	if (errno == EINPROGRESS) {
		t->whandler = handler;
		return FFAIO_ASYNC;
	}

	t->evflags |= FFKQU_ERR;
	return FFAIO_ERROR;
}

int ffaio_cancelasync(ffaio_task *t, int op, ffaio_handler oncancel)
{
	uint w = (t->whandler != NULL);
	ffaio_handler func;

#ifdef FF_BSD
	t->ev = NULL;
#endif
	t->evflags |= FFKQU_ERR;
	errno = ECANCELED;

	if ((op & FFAIO_READ) && t->rhandler != NULL) {
		func = ((oncancel != NULL) ? oncancel : t->rhandler);
		t->rhandler = NULL;
		func(t->udata);
	}

	if ((op & FFAIO_WRITE) && w && t->whandler != NULL) {
		func = ((oncancel != NULL) ? oncancel : t->whandler);
		t->whandler = NULL;
		t->evflags |= FFKQU_ERR;
		errno = ECANCELED;
		func(t->udata);
	}

	return 0;
}


size_t ff_wtou(char *dst, size_t dst_cap, const wchar_t *src, size_t srclen, int flags)
{
	mbstate_t state;
	size_t r;
	const wchar_t *srcend = src + srclen;
	const char *dsto = dst;
	const char *dstend = dst + dst_cap;
	(void)flags;

	memset(&state, 0, sizeof(mbstate_t));
	for (;  src != srcend;  src++)
	{
		if (dst + 4 > dstend)
			return 0;
		r = wcrtomb(dst, *src, &state);
		if (r == 0 || r == (size_t)-1)
			return 0;
		dst += r;
	}

	return dst - dsto;
}

#include <FFOS/time.h>
#include <FFOS/file.h>
#include <FFOS/dir.h>
#include <FFOS/thread.h>
#include <FFOS/timer.h>

#if !defined FF_NOTHR && defined FF_BSD
#include <pthread_np.h>
#endif

#include <sys/fcntl.h>
#include <string.h>

int fffile_nblock(fffd fd, int nblock)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (nblock != 0)
		flags |= O_NONBLOCK;
	else
		flags &= ~O_NONBLOCK;
	return fcntl(fd, F_SETFL, flags);
}

static const ffsyschar * fullPath(ffdirentry *ent, ffsyschar *nm, size_t nmlen) {
	if (ent->pathlen + nmlen + sizeof('/') >= ent->pathcap) {
		errno = EOVERFLOW;
		return NULL;
	}
	ent->path[ent->pathlen] = FFPATH_SLASH;
	memcpy(ent->path + ent->pathlen + 1, nm, nmlen);
	ent->path[ent->pathlen + nmlen + 1] = '\0';
	return ent->path;
}

fffileinfo * ffdir_entryinfo(ffdirentry *ent)
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

#ifdef FF_BSD
int fftmr_start(fftmr tmr, fffd qu, void *data, int periodMs)
{
	struct kevent kev;
	int f = 0;

	if (periodMs < 0) {
		periodMs = -periodMs;
		f = EV_ONESHOT;
	}

	EV_SET(&kev, tmr, EVFILT_TIMER
		, EV_ADD | EV_ENABLE | f //EV_CLEAR is set internally
		, 0, periodMs, data);
	return kevent(qu, &kev, 1, NULL, 0, NULL);
}
#endif

#ifdef FF_LINUX
int fftmr_start(fffd tmr, fffd qu, void *data, int periodMs)
{
	struct itimerspec its;
	int period = periodMs >= 0 ? periodMs : -periodMs;

	struct epoll_event e;
	e.events = EPOLLIN | EPOLLET;
	e.data.ptr = data;
	if (0 != epoll_ctl(qu, EPOLL_CTL_ADD, tmr, &e))
		return -1;

	its.it_value.tv_sec = period / 1000;
	its.it_value.tv_nsec = (period % 1000) * 1000 * 1000;
	if (periodMs >= 0)
		its.it_interval = its.it_value;
	else
		its.it_interval.tv_sec = its.it_interval.tv_nsec = 0;

	return timerfd_settime(tmr, 0, &its, NULL);
}
#endif

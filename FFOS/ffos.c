/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/file.h>
#include <FFOS/dir.h>
#include <FFOS/time.h>
#include <FFOS/socket.h>
#include <FFOS/error.h>
#include <FFOS/asyncio.h>
#include <FFOS/atomic.h>
#include <FFOS/process.h>


int ffdir_rmakeq(ffsyschar *path, size_t off)
{
	ffsyschar *s = path + off;

	if (ffpath_slash(*s))
		s++;

	for (;; s++) {

		if (ffpath_slash(*s) || *s == '\0') {
			int er;
			ffsyschar slash = *s;

			*s = '\0';
			er = ffdir_makeq(path);
			*s = slash;

			if (er != 0 && fferr_last() != EEXIST)
				return er;

			if (*s == '\0')
				break;
		}
	}

	return 0;
}


#define year_leap(year)  (((year) % 4) == 0 && (((year) % 100) || ((year) % 400) == 0))

/** Get days passed after this year. */
#define year_days(year)  ((year) * 365 + (year) / 4 - (year) / 100 + (year) / 400)

/** Get year days passed before this month (1: March). */
#define mon_ydays(mon)  (367 * (mon) / 12 - 30)

static const byte month_days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

ffbool fftime_chk(const ffdtm *dt, uint flags)
{
	if ((flags == FFTIME_CHKBOTH || (flags & FFTIME_CHKDATE))
		&& !((uint)dt->month - 1 < 12
			&& dt->weekday <= 6
			// && (dt->yday - 1 < 365 || (dt->yday == 366 && year_leap(dt->year)))
			&& ((uint)dt->day - 1 < month_days[dt->month - 1]
				|| (dt->day == 29 && dt->month == 2 && year_leap(dt->year)))))
		return 0;

	if ((flags == FFTIME_CHKBOTH || (flags & FFTIME_CHKTIME))
		&& (dt->hour > 23
			|| dt->min > 59
			|| dt->sec > 59))
		return 0;
	return 1;
}

void fftime_norm(ffdtm *dt)
{
	if (dt->msec >= 1000) {
		dt->sec += dt->msec / 1000;
		dt->msec %= 1000;
	}
	if (dt->sec >= 60) {
		dt->min += dt->sec / 60;
		dt->sec %= 60;
	}
	if (dt->min >= 60) {
		dt->hour += dt->min / 60;
		dt->min %= 60;
	}
}

int fftime_cmp(const fftime *t1, const fftime *t2)
{
	if (t1->s == t2->s) {
		if (t1->mcs == t2->mcs)
			return 0;
		if (t1->mcs < t2->mcs)
			return -1;
	}
	else if (t1->s < t2->s)
		return -1;
	return 1;
}

void fftime_totm(struct tm *tt, const ffdtm *dt)
{
	FF_ASSERT(dt->year >= 1970);
	tt->tm_year = dt->year - 1900;
	tt->tm_mon = dt->month - 1;
	tt->tm_mday = dt->day;
	tt->tm_hour = dt->hour;
	tt->tm_min = dt->min;
	tt->tm_sec = dt->sec;
}

void fftime_fromtm(ffdtm *dt, const struct tm *tt)
{
	dt->year = tt->tm_year + 1900;
	dt->month = tt->tm_mon + 1;
	dt->day = tt->tm_mday;
	dt->weekday = tt->tm_wday;
	dt->yday = tt->tm_yday + 1;
	dt->hour = tt->tm_hour;
	dt->min = tt->tm_min;
	dt->sec = tt->tm_sec;
	dt->msec = 0;
}

void fftime_addms(fftime *t, uint64 ms)
{
	t->mcs += (ms % 1000) * 1000;
	t->s += (uint)(ms / 1000);
	if (t->mcs >= 1000000) {
		t->mcs -= 1000000;
		t->s++;
	}
}

void fftime_add(fftime *t, const fftime *t2)
{
	t->s += t2->s;
	t->mcs += t2->mcs;
	if (t->mcs >= 1000000) {
		t->mcs -= 1000000;
		t->s++;
	}
}

void fftime_diff(const fftime *start, fftime *stop)
{
	if (stop->mcs >= start->mcs) {
		stop->s -= start->s;
		stop->mcs -= start->mcs;
	}
	else {
		stop->s -= start->s + 1;
		stop->mcs += 1000000 - start->mcs;
	}
}

/*
Split timestamp algorithm:
. Get day of week (1/1/1970 was Thursday).
. Get year and the days passed since its Mar 1:
 . Convert days since 1970 to days passed since Mar 1, 1 AD
 . Get approximate year (days / ~365.25).  This will be 1969 for Jan..Feb 1970.
 . Get days passed during this year.
   If all days are passed, reset days and increment year.
. Get month and its day:
 . Get month by year day
 . Get year days passed before this month
 . Get day of month
. Shift New Year from Mar 1 to Jan 1
 . If year day is within Mar..Dec, add 2 months
 . If year day is within Jan..Feb, also increment year (1969 -> 1970)
*/
void fftime_split(ffdtm *dt, const fftime *t, enum FF_TIMEZONE tz)
{
	if (tz == FFTIME_TZLOCAL) {
		struct tm tm;
		time_t tt = t->s;
#if defined FF_MSVC || defined FF_MINGW
		localtime_s(&tm, &tt);
#else
		localtime_r(&tt, &tm);
#endif
		fftime_fromtm(dt, &tm);
		dt->msec = t->mcs / 1000;
		return;
	}

	uint year, mon, days, sec, yday, mday;

	days = t->s / (60*60*24);
	sec = t->s % (60*60*24);
	dt->weekday = (4 + days) % 7;

	days += year_days(1970 - 1) - (31 + 28);
	year = 1 + days * 400 / year_days(400);
	yday = days - year_days(year - 1) - year_leap(year);
	if (yday == 365U + year_leap(year + 1)) {
		yday = 0;
		year++;
	}

	mon = (yday + 31) * 10 / 306; //1: March
	mday = yday - mon_ydays(mon);

	if (yday >= 306) { //306: days from Mar before Jan
		year++;
		mon -= 10;
		yday -= 306;
	} else {
		mon += 2;
		yday += (31 + 28);
	}

	dt->year = year;
	dt->month = mon;
	dt->yday = yday + 1;
	dt->day = mday + 1;

	dt->hour = sec / (60*60);
	dt->min = (sec % (60*60)) / 60;
	dt->sec = sec % 60;
	dt->msec = t->mcs / 1000;
}

fftime* fftime_join(fftime *t, const ffdtm *dt, enum FF_TIMEZONE tz)
{
	uint year, mon, days;

	if (tz == FFTIME_TZNODATE) {
		days = 0;
		goto set;
	}

#ifndef FF_LINUX
	if (tz == FFTIME_TZLOCAL) {
		struct tm tm;
		time_t tt;
		fftime_totm(&tm, dt);
		tt = mktime(&tm);
		t->s = (uint)tt;
		t->mcs = dt->msec * 1000;
		return t;
	}
#endif

	if (dt->year < 1970) {
		t->s = t->mcs = 0;
		return t;
	}

	year = dt->year;
	mon = dt->month - 2; //Jan -> Mar
	if ((int)mon <= 0) {
		mon += 12;
		year--;
	}
	days = year_days(year) - year_days(1970)
		+ mon_ydays(mon) + (31 + 28)
		+ dt->day - 1;

set:
	t->s = days * 60*60*24 + dt->hour * 60*60 + dt->min * 60 + dt->sec;
	t->mcs = dt->msec * 1000;

#ifdef FF_LINUX
	if (tz == FFTIME_TZLOCAL) {
		tzset();
		t->s += timezone;
		if (daylight)
			t->s -= 60*60;
		if ((int)t->s < 0)
			t->s = 0;
	}
#endif

	return t;
}


#if defined FF_WIN || (defined FF_LINUX && defined FF_OLDLIBC)
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

size_t ffiov_shiftv(ffiovec *iovs, size_t nels, uint64 *len)
{
	size_t i;
	for (i = 0;  i != nels;  i++) {
		if (*len < iovs[i].iov_len) {
			ffiov_shift(&iovs[i], (size_t)*len);
			*len = 0;
			break;
		}
		*len -= iovs[i].iov_len;
	}
	return i;
}

size_t ffiov_copyhdtr(ffiovec *dst, size_t cap, const sf_hdtr *hdtr)
{
	size_t n = 0;
	int i;
	for (i = 0; i < hdtr->hdr_cnt && n < cap; i++) {
		dst[n++] = hdtr->headers[i];
	}
	for (i = 0; i < hdtr->trl_cnt && n < cap; i++) {
		dst[n++] = hdtr->trailers[i];
	}
	return n;
}


/*
AIO:
1. ffaio_attach()
2. ffaio_*() -> non-blocking I/O -> EAGAIN -> set ffaio_task.handler, begin async operation (Windows)
3. signal from kqueue/epoll/IOCP -> ffkev_call() -> ffaio_task.handler() <-> ffaio_*() -> real I/O

Cancellation on UNIX:
ffaio_cancelasync() -> ffaio_task.handler() <-> ffaio_*() (errno=ECANCELED)

Cancellation on Windows:
1. ffaio_cancelasync() -> cancel async operation
2. signal from IOCP -> ffkev_call() -> ffaio_task.handler() <-> ffaio_*() (errno=ECANCELED)
*/

static void _ffaio_run1(ffkqu_entry *e)
{
	size_t udata = (size_t)ffkqu_data(e);
	ffaio_task *t = (void*)(udata & ~1);
	ffaio_handler func;
	uint r, w;

	uint evflags = _ffaio_events(t, e);
	r = 0 != (evflags & FFKQU_READ);
	w = 0 != (evflags & FFKQU_WRITE);

	FFDBG_PRINTLN(FFDBG_KEV | 10, "task:%p, fd:%I, evflags:%xu, r:%u, w:%u, rhandler:%p, whandler:%p, stale:%u"
		, t, (size_t)t->fd, (int)evflags, r, w, t->rhandler, t->whandler, (udata & 1) != t->instance);

	if ((udata & 1) != t->instance)
		return; //cached event has signaled

	if (r && t->rhandler != NULL) {
		func = t->rhandler;
		t->rhandler = NULL;
		t->ev = e;
		func(t->udata);
	}

	if (w && t->whandler != NULL) {
		func = t->whandler;
		t->whandler = NULL;
		t->ev = e;
		func(t->udata);
	}
}

void ffkev_call(ffkqu_entry *e)
{
	size_t udata = (size_t)ffkqu_data(e);
	ffkevent *kev = (void*)(udata & ~1);

	if (kev->aiotask) {
		_ffaio_run1(e);
		return;
	}

	uint evflags;
#if defined FF_LINUX
	evflags = e->events;
#elif defined FF_BSD
	evflags = e->flags;
#else
	evflags = 0;
#endif
	(void)evflags;
	FFDBG_PRINTLN(FFDBG_KEV | 10, "task:%p, fd:%I, evflags:%xu, handler:%p, stale:%u"
		, kev, (size_t)kev->fd, evflags, kev->handler, (udata & 1) != kev->side);

	if ((udata & 1) != kev->side) {
		/* kev.side is modified in ffkev_fin() every time an event is closed or reused for another operation.
		This event was cached in userspace and it's no longer valid. */
		return;
	}

	if (kev->handler != NULL) {
		ffkev_handler func = kev->handler;
		if (kev->oneshot)
			kev->handler = NULL;
		func(kev->udata);
	}
}


uint _ffsc_ncpu;

void fflk_setup(void)
{
	ffsysconf sc;
	if (_ffsc_ncpu != 0)
		return;
	ffsc_init(&sc);
	_ffsc_ncpu = ffsc_get(&sc, _SC_NPROCESSORS_ONLN);
}

enum { FFLK_SPIN = 2048 };

void fflk_lock(fflock *lk)
{
	for (;;) {
		if (fflk_trylock(lk))
			return;

		if (_ffsc_ncpu > 1) {
			uint n;
			for (n = 0;  n < FFLK_SPIN;  n++) {
				ffcpu_pause();
				if (fflk_trylock(lk))
					return;
			}
		}

		ffcpu_yield();
	}
}


static const char *const err_ops[] = {
	"success"
	, "internal error"

	, "read"
	, "write"

	, "buffer alloc"
	, "buffer grow"

	, "file open"
	, "file seek"
	, "file map"
	, "file remove"
	, "file close"
	, "file rename"

	, "directory open"
	, "directory make"

	, "timer init"
	, "kqueue create"
	, "kqueue attach"
	, "kqueue wait"
	, "thread create"
	, "non-blocking mode set"
	, "process fork"

	, "library open"
	, "library symbol address"

	, "socket create"
	, "socket bind"
	, "socket option"
	, "socket shutdown"
	, "socket listen"
	, "socket connect"
	, "address resolve"

	, "system error"
};

const char * fferr_opstr(enum FFERR e)
{
	FF_ASSERT(e <= FFERR_SYSTEM);
	return err_ops[e];
}


#ifdef FFMEM_DBG
static ffatomic _ffmem_total; //accumulated size of (re)allocated memory; NOT the currently allocated memory size
void* ffmem_alloc(size_t size)
{
	void *p = _ffmem_alloc(size);
	ffatom_add(&_ffmem_total, size);
	FFDBG_PRINTLN(FFDBG_MEM | 10, "p:%p, size:%L [%LK]", p, size, _ffmem_total / 1024);
	return p;
}

void* ffmem_calloc(size_t n, size_t sz)
{
	void *p = _ffmem_calloc(n, sz);
	ffatom_add(&_ffmem_total, n * sz);
	FFDBG_PRINTLN(FFDBG_MEM | 10, "p:%p, size:%L*%L [%LK]", p, n, sz, _ffmem_total / 1024);
	return p;
}

void* ffmem_realloc(void *ptr, size_t newsize)
{
	void *p;
	p = _ffmem_realloc(ptr, newsize);
	ffatom_add(&_ffmem_total, newsize);
	FFDBG_PRINTLN(FFDBG_MEM | 10, "p:%p, size:%L, oldp:%p [%LK]", p, newsize, ptr, _ffmem_total / 1024);
	return p;
}

void ffmem_free(void *ptr)
{
	FFDBG_PRINTLN(FFDBG_MEM | 10, "p:%p", ptr);
	_ffmem_free(ptr);
}

void* ffmem_align(size_t size, size_t align)
{
	void *p = _ffmem_align(size, align);
	ffatom_add(&_ffmem_total, size);
	FFDBG_PRINTLN(FFDBG_MEM | 10, "p:%p, size:%L, align:%L [%LK]", p, size, align, _ffmem_total / 1024);
	return p;
}

void ffmem_alignfree(void *ptr)
{
	FFDBG_PRINTLN(FFDBG_MEM | 10, "p:%p", ptr);
	_ffmem_alignfree(ptr);
}
#endif //FFMEM_DBG


#if defined _DEBUG && defined FF_FFOS_ONLY
int ffdbg_mask;
int ffdbg_print(int t, const char *fmt, ...)
{
	return 0;
}
#endif

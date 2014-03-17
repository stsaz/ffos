/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/file.h>
#include <FFOS/time.h>
#include <FFOS/socket.h>
#include <FFOS/error.h>
#include <FFOS/asyncio.h>


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


void ffaio_run1(ffkqu_entry *e)
{
	size_t udata = (size_t)ffkqu_data(e);
	ffaio_task *t = (void*)(udata & ~1);
	ffaio_handler func;
	unsigned r, w;

#ifdef FF_UNIX
	if ((udata & 1) != t->instance)
		return; //cached event has signaled
#endif

	t->evflags = _ffaio_events(t, e);
	r = 0 != (t->evflags & FFKQU_READ);
	w = 0 != (t->evflags & FFKQU_WRITE);
#ifdef FF_BSD
	t->ev = e;
#endif

#ifdef FFDBG_AIO
	ffdbg_print(0, __FUNCTION__ "(): task:%p, evflags:%u, r:%u, w:%u, rhandler:%p, whandler:%p\n"
		, t, (int)t->evflags, r, w, t->rhandler, t->whandler);
#endif

	if (r && t->rhandler != NULL) {
		func = t->rhandler;
		t->rhandler = NULL;
		func(t->udata);
	}

	if (w && t->whandler != NULL) {
		func = t->whandler;
		t->whandler = NULL;
		func(t->udata);
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

	, "timer init"
	, "kqueue create"
	, "kqueue attach"
	, "kqueue wait"
	, "thread create"
	, "non-blocking mode set"
	, "process fork"

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

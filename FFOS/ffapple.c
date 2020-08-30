/**
Copyright (c) 2017 Simon Zolin
*/

#include <FFOS/dir.h>
#include <FFOS/socket.h>
#include <FFOS/process.h>
#include <FFOS/sig.h>
#include <FFOS/timer.h>

#include <sys/wait.h>
#include <mach-o/dyld.h>


int ffpipe_create2(fffd *rd, fffd *wr, uint flags)
{
	if (0 != ffpipe_create(rd, wr))
		return -1;

	if ((flags & FFO_NONBLOCK)
		&& (0 != ffpipe_nblock(*rd, 1)
			|| 0 != ffpipe_nblock(*wr, 1))) {
		ffpipe_close(*rd);
		ffpipe_close(*wr);
		return -1;
	}

	return 0;
}


void fftime_local(fftime_zone *tz)
{
	tzset();
	struct tm tm;
	time_t t = time(NULL);
	localtime_r(&t, &tm);
	tz->off = tm.tm_gmtoff;
	tz->have_dst = 0;
}


int fftmr_start(fftmr tmr, fffd kq, void *udata, int period_ms)
{
	struct kevent kev;
	int f = 0;

	if (period_ms < 0) {
		period_ms = -period_ms;
		f = EV_ONESHOT;
	}

	EV_SET(&kev, tmr, EVFILT_TIMER
		, EV_ADD | EV_ENABLE | f //EV_CLEAR is set internally
		, 0, period_ms, udata);
	return kevent(kq, &kev, 1, NULL, 0, NULL);
}

int ffsig_ctl(ffsignal *t, fffd kq, const int *sigs, size_t nsigs, ffaio_handler handler)
{
	size_t i;
	struct kevent *evs;
	int r = 0;
	void *udata;
	int f = EV_ADD | EV_ENABLE;

	if (handler == NULL) {
		f = EV_DELETE;
	}

	evs = ffmem_allocT(nsigs, struct kevent);
	if (evs == NULL)
		return -1;

	t->rhandler = handler;
	t->oneshot = 0;
	udata = ffaio_kqudata(t);
	for (i = 0;  i < nsigs;  i++) {
		EV_SET(&evs[i], sigs[i], EVFILT_SIGNAL, f, 0, 0, udata);
	}
	if (0 != kevent(kq, evs, nsigs, NULL, 0, NULL))
		r = -1;

	ffmem_free(evs);

	if (handler == NULL)
		ffaio_fin(t);
	return r;
}


ffskt ffskt_create(uint domain, uint type, uint protocol)
{
	ffskt sk = socket(domain, type & ~SOCK_NONBLOCK, protocol);
	if ((type & SOCK_NONBLOCK) && sk != FF_BADSKT && 0 != ffskt_nblock(sk, 1)) {
		ffskt_close(sk);
		sk = FF_BADSKT;
	}
	return sk;
}

/* sendfile() sends the whole file if 'file size' argument is 0.
We don't want this behaviour. */
int ffskt_sendfile(ffskt sk, fffd fd, uint64 offs, uint64 sz, sf_hdtr *hdtr, uint64 *_sent, int flags)
{
	off_t sent = 0;
	ssize_t r;

	if (sz != 0) {
		sent = FF_TOSIZE(sz);
		if (hdtr != NULL) {
			if (hdtr->hdr_cnt == 0)
				hdtr->headers = NULL;
			sent += ffiov_size(hdtr->headers, hdtr->hdr_cnt);
		}
		r = sendfile(fd, sk, offs, &sent, hdtr, flags);
		if (r != 0 && !fferr_again(fferr_last()))
			return -1;
		goto done;
	}

	if (hdtr == NULL) {
		r = 0;
		goto done; //no file, no hdtr
	}

	if (hdtr->hdr_cnt != 0) {
		r = ffskt_sendv(sk, hdtr->headers, hdtr->hdr_cnt);
		if (r == -1)
			goto done;

		sent = r;

		if (hdtr->trl_cnt != 0
			&& (size_t)r != ffiov_size(hdtr->headers, hdtr->hdr_cnt))
			goto done; //headers are not sent yet completely
	}

	if (hdtr->trl_cnt != 0) {
		r = ffskt_sendv(sk, hdtr->trailers, hdtr->trl_cnt);
		if (r == -1)
			goto done;

		sent += r;
	}

	r = 0;

done:
	*_sent = sent;
	return (int)r;
}


ssize_t ffaio_fwrite(ffaio_filetask *ft, const void *data, size_t len, uint64 off, ffaio_handler handler)
{
	return fffile_pwrite(ft->kev.fd, data, len, off);
}

ssize_t ffaio_fread(ffaio_filetask *ft, void *data, size_t len, uint64 off, ffaio_handler handler)
{
	return fffile_pread(ft->kev.fd, data, len, off);
}

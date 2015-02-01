/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/socket.h>
#include <FFOS/process.h>
#include <FFOS/sig.h>
#include <FFOS/timer.h>

#include <sys/wait.h>


int ffps_wait(fffd h, uint timeout, int *exit_code)
{
	int st = 0;
	(void)timeout;

	do {
		if (-1 == waitpid(h, &st, WNOWAIT))
			return -1;
	} while (!WIFEXITED(st) && !WIFSIGNALED(st)); // exited or killed

	if (exit_code != NULL)
		*exit_code = (WIFEXITED(st) ? WEXITSTATUS(st) : -1);

	return 0;
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

int ffsig_ctl(ffaio_task *t, fffd kq, const int *sigs, size_t nsigs, ffaio_handler handler)
{
	size_t i;
	struct kevent *evs;
	int r = 0;
	void *udata;
	int f = EV_ADD | EV_ENABLE;

	if (handler == NULL) {
		if (kq == FF_BADFD || nsigs == 0)
			return 0;
		f = EV_DELETE;
	}

	evs = ffmem_talloc(struct kevent, nsigs);
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
	return r;
}

/* sendfile() sends the whole file if 'file size' argument is 0.
We don't want this behaviour. */
int ffskt_sendfile(ffskt sk, fffd fd, uint64 offs, uint64 sz, sf_hdtr *hdtr, uint64 *_sent, int flags)
{
	off_t sent = 0;
	ssize_t r;

	if (sz != 0) {
		r = sendfile(fd, sk, /*(off_t)*/offs, FF_TOSIZE(sz), hdtr, &sent, flags);
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
			&& r != ffiov_size(hdtr->headers, hdtr->hdr_cnt))
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

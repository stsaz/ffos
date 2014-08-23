#include <FFOS/file.h>
#include <FFOS/time.h>
#include <FFOS/error.h>
#include <FFOS/process.h>
#include <FFOS/sig.h>
#include <FFOS/timer.h>
#include <FFOS/socket.h>

#include <time.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/sendfile.h>

int fffile_time(fffd fd, fftime *last_write, fftime *last_access, fftime *cre)
{
	struct stat s;
	if (0 != fstat(fd, &s))
		return -1;

	if (last_access)
		fftime_fromtime_t(last_access, s.st_atime);
	if (last_write)
		fftime_fromtime_t(last_write, s.st_mtime);
	if (cre)
		cre->s = cre->mcs = 0;
	return 0;
}

int fferr_str(int code, char *dst, size_t dst_cap) {
	char *dst2;
	if (0 == dst_cap)
		return 0;
	dst2 = strerror_r(code, dst, dst_cap);
	if (dst2 != dst) {
		ssize_t len = ffmin(dst_cap - 1, strlen(dst2));
		memcpy(dst, dst2, len);
		dst[len] = '\0';
		return len;
	}
	return strlen(dst);
}

int ffps_wait(fffd h, uint timeout, int *exit_code)
{
	siginfo_t s;
	int r;
	(void)timeout;

	r = waitid(P_PID, (id_t)h, &s, WEXITED | WNOWAIT);
	if (r != 0)
		return -1;

	if (exit_code != NULL)
		*exit_code = s.si_status;
	return 0;
}

int fftmr_start(fftmr tmr, fffd kq, void *udata, int period_ms)
{
	struct itimerspec its;
	struct epoll_event e;
	unsigned neg = 0;

	e.events = EPOLLIN | EPOLLET;
	e.data.ptr = udata;
	if (0 != epoll_ctl(kq, EPOLL_CTL_ADD, tmr, &e))
		return -1;

	if (period_ms < 0) {
		neg = 1;
		period_ms = -period_ms;
		its.it_interval.tv_sec = its.it_interval.tv_nsec = 0;
	}

	its.it_value.tv_sec = period_ms / 1000;
	its.it_value.tv_nsec = (period_ms % 1000) * 1000 * 1000;
	if (!neg)
		its.it_interval = its.it_value;

	return timerfd_settime(tmr, 0, &its, NULL);
}

int ffskt_sendfile(ffskt sk, fffd fd, uint64 offs, uint64 sz, sf_hdtr *hdtr, uint64 *_sent, int flags)
{
	int npush = 0;
	ssize_t res = 0;
	ssize_t r;
	uint64 sent = 0;
	(void)flags;

	if (hdtr != NULL && (hdtr->hdr_cnt | hdtr->trl_cnt) != 0
		&& 0 == ffskt_setopt(sk, IPPROTO_TCP, TCP_NOPUSH, 1))
		npush = 1;

	if (hdtr != NULL && hdtr->hdr_cnt != 0) {
		size_t hsz = ffiov_size(hdtr->headers, hdtr->hdr_cnt);
		r = ffskt_sendv(sk, hdtr->headers, hdtr->hdr_cnt);
		if (r == -1) {
			res = -1;
			goto end;
		}

		sent = r;
		if (r != hsz)
			goto end;
	}

	if (sz != 0) {
		off64_t foffset = offs;
		r = sendfile64(sk, fd, &foffset, FF_TOSIZE(sz));
		if (r == -1) {
			res = -1;
			goto end;
		}

		sent += r;
		if ((uint64)r != sz)
			goto end;
	}

	if (hdtr != NULL && hdtr->trl_cnt != 0) {
		r = ffskt_sendv(sk, hdtr->trailers, hdtr->trl_cnt);
		if (r == -1) {
			res = -1;
			goto end;
		}

		sent += r;
	}

end:
	if (npush)
		(void)ffskt_setopt(sk, IPPROTO_TCP, TCP_NOPUSH, 0);

	*_sent = sent;
	return res;
}

int ffsig_ctl(ffaio_task *t, fffd kq, const int *sigs, size_t nsigs, ffaio_handler handler)
{
	sigset_t mask;
	size_t i;
	fffd sigfd;

	if (handler == NULL) {
		if (t->fd != FF_BADFD) {
			close(t->fd);
			t->fd = FF_BADFD;
		}
		return 0;
	}

	sigemptyset(&mask);
	for (i = 0;  i < nsigs;  i++) {
		sigaddset(&mask, sigs[i]);
	}

	sigfd = signalfd(FF_BADFD, &mask, SFD_NONBLOCK);
	if (sigfd == FF_BADFD)
		return -1;

	t->rhandler = handler;
	t->oneshot = 0;
	if (0 != ffkqu_attach(kq, sigfd, ffaio_kqudata(t), FFKQU_ADD | FFKQU_READ)) {
		close(sigfd);
		return -1;
	}

	t->fd = sigfd;
	return 0;
}

/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/socket.h>
#include <FFOS/process.h>
#include <FFOS/sig.h>
#include <FFOS/timer.h>

#include <sys/wait.h>
#include <sys/sysctl.h>


extern const char* _ffpath_real(char *name, size_t cap, const char *argv0);

#ifdef KERN_PROC_PATHNAME
static const int sysctl_pathname[] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, /*PID=*/ -1 };
#endif

const char* ffps_filename(char *name, size_t cap, const char *argv0)
{
#ifdef KERN_PROC_PATHNAME
	size_t namelen = cap;
	if (0 == sysctl(sysctl_pathname, FFCNT(sysctl_pathname), name, &namelen, NULL, 0))
		return name;
#endif

	int n = readlink("/proc/curproc/file", name, cap);
	if (n >= 0) {
		name[n] = '\0';
		return name;
	}

	n = readlink("/proc/curproc/exe", name, cap);
	if (n >= 0) {
		name[n] = '\0';
		return name;
	}

	return _ffpath_real(name, cap, argv0);
}


enum {
	KQ_EVUSER_ID = 1,
};

int ffkqu_post_attach(ffkevpost *p, fffd kq)
{
	struct kevent ev;

	ffkev_init(p);
	EV_SET(&ev, KQ_EVUSER_ID, EVFILT_USER, EV_ADD | EV_ENABLE | EV_ONESHOT | EV_CLEAR, 0, 0, NULL);
	if (0 != kevent(kq, &ev, 1, NULL, 0, NULL))
		return -1;
	p->fd = kq;
	return 0;
}

void ffkqu_post_detach(ffkevpost *p, fffd kq)
{
	struct kevent ev;
	EV_SET(&ev, KQ_EVUSER_ID, EVFILT_USER, EV_DELETE, 0, 0, NULL);
	kevent(kq, &ev, 1, NULL, 0, NULL);
}

int ffkqu_post(ffkevpost *p, void *data)
{
	fffd kq = p->fd;
	struct kevent ev;
	EV_SET(&ev, KQ_EVUSER_ID, EVFILT_USER, 0, NOTE_TRIGGER, 0, data);
	return kevent(kq, &ev, 1, NULL, 0, NULL);
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
	return socket(domain, type, protocol);
}

/* sendfile() sends the whole file if 'file size' argument is 0.
We don't want this behaviour. */
int ffskt_sendfile(ffskt sk, fffd fd, uint64 offs, uint64 sz, sf_hdtr *hdtr, uint64 *_sent, int flags)
{
	off_t sent = 0;
	ssize_t r;

	if (sz != 0) {
		r = sendfile(fd, sk, /*(off_t)*/offs, FF_TOSIZE(sz), hdtr, &sent, flags);
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


static ssize_t _ffaio_fresult(ffaio_filetask *ft)
{
	int r = aio_error(&ft->acb);
	if (r == EINPROGRESS) {
		errno = EAGAIN;
		return -1;
	}

	ft->kev.pending = 0;
	if (r == -1)
		return -1;

	return aio_return(&ft->acb);
}

static void _ffaio_fprepare(ffaio_filetask *ft, void *data, size_t len, uint64 off)
{
	fffd kq;
	struct aiocb *acb = &ft->acb;

	kq = acb->aio_sigevent.sigev_notify_kqueue;
	ffmem_tzero(acb);
	acb->aio_sigevent.sigev_notify_kqueue = kq;
	/* kevent.filter == EVFILT_AIO
	(struct aiocb *)kevent.ident */
	acb->aio_sigevent.sigev_notify = SIGEV_KEVENT;
	acb->aio_sigevent.sigev_notify_kevent_flags = EV_CLEAR;
	acb->aio_sigevent.sigev_value.sigval_ptr = ffkev_ptr(&ft->kev);

	acb->aio_fildes = ft->kev.fd;
	acb->aio_buf = data;
	acb->aio_nbytes = len;
	acb->aio_offset = off;
}

ssize_t ffaio_fwrite(ffaio_filetask *ft, const void *data, size_t len, uint64 off, ffaio_handler handler)
{
	ssize_t r;

	if (ft->kev.pending)
		return _ffaio_fresult(ft);

	if (ft->acb.aio_sigevent.sigev_notify_kqueue == FF_BADFD)
		goto wr;

	_ffaio_fprepare(ft, (void*)data, len, off);
	ft->acb.aio_lio_opcode = LIO_WRITE;
	if (0 != aio_write(&ft->acb)) {
		if (errno == EAGAIN) //no resources for this I/O operation
			goto wr;
		else if (errno == ENOSYS || errno == EOPNOTSUPP) {
			ft->acb.aio_sigevent.sigev_notify_kqueue = FF_BADFD;
			goto wr;
		}
		return -1;
	}

	ft->kev.pending = 1;
	r = _ffaio_fresult(ft);
	if (ft->kev.pending)
		ft->kev.handler = handler;
	return r;

wr:
	return fffile_pwrite(ft->kev.fd, data, len, off);
}

/* If aio_read() doesn't work, use pread(). */
ssize_t ffaio_fread(ffaio_filetask *ft, void *data, size_t len, uint64 off, ffaio_handler handler)
{
	ssize_t r;

	if (ft->kev.pending)
		return _ffaio_fresult(ft);

	if (ft->acb.aio_sigevent.sigev_notify_kqueue == FF_BADFD)
		goto rd;

	_ffaio_fprepare(ft, data, len, off);
	ft->acb.aio_lio_opcode = LIO_READ;
	if (0 != aio_read(&ft->acb)) {
		if (errno == EAGAIN) //no resources for this I/O operation
			goto rd;
		else if (errno == ENOSYS || errno == EOPNOTSUPP) {
			ft->acb.aio_sigevent.sigev_notify_kqueue = FF_BADFD;
			goto rd;
		}
		return -1;
	}

	ft->kev.pending = 1;
	r = _ffaio_fresult(ft);
	if (ft->kev.pending)
		ft->kev.handler = handler;
	return r;

rd:
	return fffile_pread(ft->kev.fd, data, len, off);
}

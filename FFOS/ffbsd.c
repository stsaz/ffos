/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/types.h>
#include <FFOS/dir.h>
#include <FFOS/socket.h>
#include <FFOS/process.h>
#include <FFOS/sig.h>
#include <FFOS/timer.h>

#include <sys/wait.h>
#include <sys/sysctl.h>


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

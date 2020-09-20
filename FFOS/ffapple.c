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


void fftime_local(fftime_zone *tz)
{
	tzset();
	struct tm tm;
	time_t t = time(NULL);
	localtime_r(&t, &tm);
	tz->off = tm.tm_gmtoff;
	tz->have_dst = 0;
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
	(void)handler;
	return fffile_pwrite(ft->kev.fd, data, len, off);
}

ssize_t ffaio_fread(ffaio_filetask *ft, void *data, size_t len, uint64 off, ffaio_handler handler)
{
	(void)handler;
	return fffile_pread(ft->kev.fd, data, len, off);
}

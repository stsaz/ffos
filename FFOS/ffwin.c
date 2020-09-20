/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/dir.h>
#include <FFOS/time.h>
#include <FFOS/thread.h>
#include <FFOS/timer.h>
#include <FFOS/process.h>
#include <FFOS/asyncio.h>
#include <FFOS/sig.h>
#include <FFOS/atomic.h>
#include <FFOS/semaphore.h>
#include <FFOS/dylib.h>
#include <FFOS/pipe.h>

#include <psapi.h>
#include <time.h>
#ifndef FF_MSVC
#include <stdio.h>
#endif
#include <winternl.h>
#include <ntstatus.h>


int _ffaio_events(ffaio_task *t, const ffkqu_entry *e)
{
	int ev = 0;

	if (e->lpOverlapped == &t->wovl)
		ev |= FFKQU_WRITE;
	else
		ev |= FFKQU_READ;

	return ev;
}

ssize_t ffaio_fwrite(ffaio_filetask *ft, const void *data, size_t len, uint64 off, ffaio_handler handler)
{
	BOOL b;

	if (ft->kev.pending) {
		ft->kev.pending = 0;
		return ffio_result(&ft->kev.ovl);
	}

	ffmem_tzero(&ft->kev.ovl);
	ft->kev.ovl.Offset = (uint)off;
	ft->kev.ovl.OffsetHigh = (uint)(off >> 32);
	b = WriteFile(ft->kev.fd, data, FF_TOINT(len), NULL, &ft->kev.ovl);

	if (0 != fferr_ioret(b))
		return -1;

	if (!ft->kev.faio_direct)
		return ffio_result(&ft->kev.ovl);

	ft->kev.pending = 1;
	ft->kev.handler = handler;
	fferr_set(EAGAIN);
	return -1;
}

ssize_t ffaio_fread(ffaio_filetask *ft, void *data, size_t len, uint64 off, ffaio_handler handler)
{
	BOOL b;

	if (ft->kev.pending) {
		ft->kev.pending = 0;
		int r = ffio_result(&ft->kev.ovl);
		if (r < 0 && fferr_last() == ERROR_HANDLE_EOF)
			return 0;
		return r;
	}

	ffmem_tzero(&ft->kev.ovl);
	ft->kev.ovl.Offset = (uint)off;
	ft->kev.ovl.OffsetHigh = (uint)(off >> 32);
	b = ReadFile(ft->kev.fd, data, FF_TOINT(len), NULL, &ft->kev.ovl);

	if (0 != fferr_ioret(b)) {
		if (fferr_last() == ERROR_HANDLE_EOF)
			return 0;
		return -1;
	}

	if (!ft->kev.faio_direct)
		return ffio_result(&ft->kev.ovl);

	ft->kev.pending = 1;
	ft->kev.handler = handler;
	fferr_set(EAGAIN);
	return -1;
}


fffd ffaio_pipe_accept(ffkevent *kev, ffkev_handler handler)
{
	if (kev->pending) {
		kev->pending = 0;
		return kev->fd;
	}

	ffmem_tzero(&kev->ovl);

	BOOL b = ConnectNamedPipe(kev->fd, &kev->ovl);
	if (0 != fferr_ioret(b))
		return FF_BADFD;

	if (!b) {
		kev->pending = 1;
		kev->handler = handler;
		fferr_set(EAGAIN);
		return FF_BADFD;
	}

	return kev->fd;
}


static LARGE_INTEGER _ffclk_freq;

void ffclk_totime(fftime *t)
{
	if (_ffclk_freq.QuadPart == 0)
		QueryPerformanceFrequency(&_ffclk_freq); //no threads race because the result is always the same

	uint64 clk = t->sec;
	t->sec = clk / _ffclk_freq.QuadPart;
	uint ns = (clk % _ffclk_freq.QuadPart) * 1000000000 / _ffclk_freq.QuadPart;
	fftime_setnsec(t, ns);
}


static void pipename(char *dst, size_t cap, int pid)
{
	ffs_format(dst, cap, FFPIPE_NAME_PREFIX "ffps-%u%Z", pid);
}

static BOOL __stdcall _ffsig_ctrlhandler(DWORD ctrl)
{
	FFDBG_PRINTLN(5, "CTRL event: %u", ctrl);

	if (ctrl == CTRL_C_EVENT) {
		ffps_sig(ffps_curid(), SIGINT);
		return 1;
	}
	return 0;
}

int ffsig_ctl(ffsignal *t, fffd kq, const int *sigs, size_t nsigs, ffaio_handler handler)
{
	char name[64];
	ffbool ctrl_c = 0;
	size_t n;

	for (n = 0;  n != nsigs;  n++) {
		if (sigs[n] == SIGINT) {
			ctrl_c = 1;
			break;
		}
	}

	if (handler == NULL) {
		if (ctrl_c) {
			SetConsoleCtrlHandler(&_ffsig_ctrlhandler, 0);
		}

		if (t->fd != FF_BADFD)
			ffpipe_close(t->fd);
		ffkev_fin(t);
		return 0;
	}

	pipename(name, FFCNT(name), ffps_curid());
	t->fd = ffpipe_create_named(name, 0);
	if (t->fd == FF_BADFD)
		return 1;

	if (0 != ffkqu_attach(kq, t->fd, ffkev_ptr(t), 0))
		return 1;

	if (FF_BADFD == ffaio_pipe_accept(t, handler) && !fferr_again(fferr_last()))
		return -1;

	t->oneshot = 0;

	if (ctrl_c) {
		SetConsoleCtrlHandler(&_ffsig_ctrlhandler, 1);
	}

	return 0;
}

int ffsig_read(ffsignal *t, ffsiginfo *si)
{
	(void)si;
	ssize_t r;
	byte b;

	if (t->pending)
		ffaio_pipe_accept(t, NULL);

	for (;;) {
		r = fffile_read(t->fd, &b, 1);
		if (r == 1)
			return b;

		ffpipe_peer_close(t->fd);
		if (FF_BADFD == ffaio_pipe_accept(t, t->handler))
			return -1;
	}
}

int ffps_sig(int pid, int sig)
{
	char name[64];
	fffd p = FF_BADFD;
	byte b;

	pipename(name, FFCNT(name), pid);

	const ffuint share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
	p = CreateFileA(name, GENERIC_WRITE, share, NULL, OPEN_EXISTING, 0, NULL);
	if (p == FF_BADFD)
		return 1;

	b = (byte)sig;
	if (1 != fffile_write(p, &b, 1)) {
		ffpipe_close(p);
		return 1;
	}

	ffpipe_close(p);
	return 0;
}

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

#include <psapi.h>
#include <time.h>
#ifndef FF_MSVC
#include <stdio.h>
#endif
#include <winternl.h>
#include <ntstatus.h>


/** Create a system string and add a backslash to the end. */
static ffsyschar* _ffs_utow_bslash(const char *s)
{
	ffsyschar *w;
	size_t len = strlen(s);
	if (len != 0 && s[len - 1] == '\\')
		len--;
	size_t n = ff_utow(NULL, 0, s, len, 0);
	if (NULL == (w = ffq_alloc(n + 2)))
		return NULL;
	n = ff_utow(w, n, s, len, 0);
	w[n++] = '\\';
	w[n] = '\0';
	return w;
}

int fffile_mount(const char *disk, const char *mount)
{
	int rc = -1;
	ffsyschar *wdisk = NULL, *wmount = NULL;
	if (NULL == (wmount = _ffs_utow_bslash(mount)))
		goto end;

	if (disk == NULL) {
		if (!DeleteVolumeMountPoint(wmount))
			goto end;

	} else {
		if (NULL == (wdisk = _ffs_utow_bslash(disk)))
			goto end;
		if (!SetVolumeMountPoint(wmount, wdisk))
			goto end;
	}

	rc = 0;

end:
	ffmem_free(wdisk);
	ffmem_free(wmount);
	return rc;
}


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

fffd ffpipe_create_named(const char *name, uint flags)
{
	fffd f;
	ffsyschar *w, ws[FF_MAXFN];
	size_t n = FFCNT(ws);
	if (NULL == (w = ffs_utow(ws, &n, name, -1)))
		return FF_BADFD;

	f = ffpipe_create_namedq(w, flags);
	if (w != ws)
		ffmem_free(w);
	return f;
}

ssize_t ffpipe_read(fffd fd, void *buf, size_t cap)
{
	ssize_t r;
	if (0 > (r = fffile_read(fd, buf, cap))) {
		if (fferr_last() == ERROR_BROKEN_PIPE)
			return 0;
		else if (fferr_last() == ERROR_NO_DATA)
			fferr_set(EAGAIN);
		return -1;
	}
	return r;
}


// [/\\] | \w:[\0/\\]
ffbool ffpath_abs(const char *path, size_t len)
{
	char c;
	if (len == 0)
		return 0;

	if (ffpath_slash(path[0]))
		return 1;

	c = path[0] | 0x20; //lower
	return
		(len >= FFSLEN("c:") && c >= 'a' && c <= 'z'
		&& path[1] == ':'
		&& (len == FFSLEN("c:") || ffpath_slash(path[2]))
		);
}


#if FF_WIN < 0x0600
static VOID WINAPI (*_GetSystemTimePreciseAsFileTime)(LPFILETIME);
void fftime_init(void)
{
	_GetSystemTimePreciseAsFileTime = (VOID WINAPI (*)(LPFILETIME))ffdl_addr(GetModuleHandle(L"kernel32.dll"), "GetSystemTimePreciseAsFileTime");
}
#endif


void fftime_now(fftime *t)
{
	FILETIME ft;
#if FF_WIN >= 0x0600
	GetSystemTimePreciseAsFileTime(&ft);
#else
	if (_GetSystemTimePreciseAsFileTime != NULL)
		_GetSystemTimePreciseAsFileTime(&ft);
	else
		GetSystemTimeAsFileTime(&ft);
#endif
	*t = fftime_from_winftime((void*)&ft);
}

void fftime_local(fftime_zone *tz)
{
	tzset();
	tz->off = -timezone;
	tz->have_dst = daylight;
}


WCHAR* ffs_utow(WCHAR *dst, size_t *dstlen, const char *s, size_t len)
{
	size_t wlen;

	if (dst != NULL) {
		wlen = ff_utow(dst, *dstlen, s, len, 0);
		if (wlen != 0 || len == 0)
			goto done;
	}

	//not enough space in the provided buffer.  Allocate a new one.
	wlen = (len == (size_t)-1) ? strlen(s) + 1 : len + 1;
	dst = ffmem_allocT(wlen, WCHAR);
	if (dst == NULL)
		return NULL;

	wlen = ff_utow(dst, wlen, s, len, 0);
	if (wlen == 0) {
		ffmem_free(dst);
		return NULL;
	}

done:
	if (dstlen != NULL)
		*dstlen = wlen;
	return dst;
}


static FFINL ffsyschar * scopyz(ffsyschar *dst, const ffsyschar *sz) {
	while (*sz != '\0')
		*dst++ = *sz++;
	return dst;
}


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


/** User's signal handler. */
static ffsig_handler _ffsig_userhandler;

/** Called by OS on a program exception.
We reset to the default exception handler on entry. */
static LONG WINAPI _ffsigfunc(struct _EXCEPTION_POINTERS *inf)
{
	SetUnhandledExceptionFilter(NULL);

	struct ffsig_info i = {};
	i.sig = inf->ExceptionRecord->ExceptionCode; //EXCEPTION_*
	i.si = inf;
	switch (inf->ExceptionRecord->ExceptionCode) {
	case EXCEPTION_ACCESS_VIOLATION:
		i.flags = inf->ExceptionRecord->ExceptionInformation[0];
		i.addr = (void*)inf->ExceptionRecord->ExceptionInformation[1];
		break;
	}
	_ffsig_userhandler(&i);
	return EXCEPTION_CONTINUE_SEARCH;
}

int ffsig_subscribe(ffsig_handler handler, const uint *sigs, uint nsigs)
{
	if (handler != NULL)
		_ffsig_userhandler = handler;

	SetUnhandledExceptionFilter(&_ffsigfunc);
	return 0;
}


static void pipename(ffsyschar *dst, size_t cap, int pid)
{
	char s[64];
#ifdef FF_MSVC
	_ultoa_s(pid, s, FFCNT(s), 10);
#else
	sprintf(s, "%u", pid);
#endif
	dst = scopyz(dst, L"\\\\.\\pipe\\ffps-");
	dst += ff_utow(dst, cap, s, strlen(s), 0);
	*dst = L'\0';
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
	ffsyschar name[64];
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
	t->fd = ffpipe_create_namedq(name, 0);
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
	ffsyschar name[64];
	fffd p = FF_BADFD;
	byte b;

	pipename(name, FFCNT(name), pid);

	const ffuint share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
	p = CreateFileW(name, GENERIC_WRITE, share, NULL, OPEN_EXISTING, 0, NULL);
	if (p == FF_BADFD)
		return 1;

	b = (byte)sig;
	if (1 != fffile_write(p, &b, 1)) {
		ffpipe_client_close(p);
		return 1;
	}

	ffpipe_client_close(p);
	return 0;
}


int ffpath_infoinit(const char *path, ffpathinfo *st)
{
	DWORD spc, bps, fc, c;
	BOOL b;
	WCHAR wpath_s[256];
	size_t wpath_len = FFCNT(wpath_s);
	WCHAR *wpath = ffs_utow(wpath_s, &wpath_len, path, -1);
	if (wpath == NULL)
		return 1;

	b = GetDiskFreeSpaceW(wpath, &spc, &bps, &fc, &c);
	if (wpath != wpath_s)
		ffmem_free(wpath);
	if (!b)
		return 1;

	st->f_bsize = spc * bps;
	return 0;
}

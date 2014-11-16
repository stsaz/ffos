/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/dir.h>
#include <FFOS/time.h>
#include <FFOS/socket.h>
#include <FFOS/thread.h>
#include <FFOS/timer.h>
#include <FFOS/process.h>
#include <FFOS/asyncio.h>

#include <time.h>
#ifndef FF_MSVC
#include <stdio.h>
#endif

HANDLE _ffheap;

fffd fffile_openq(const ffsyschar *filename, int flags)
{
	enum { share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE };
	int mode = (flags & 0x0000000f);
	int access = (flags & 0x000000f0) << 24;
	int f = (flags & 0xfffffe00) | FILE_ATTRIBUTE_NORMAL;

	if ((flags & FFO_NODOSNAME) && !ffpath_islong(filename)) {
		return FF_BADFD;
	}

	if (mode == FFO_APPEND) {
		mode = OPEN_ALWAYS;
		access = STANDARD_RIGHTS_WRITE | FILE_APPEND_DATA | SYNCHRONIZE;
	}

	return CreateFile(filename, access, share, NULL, mode, f, NULL);
}

int fffile_time(fffd fd, fftime *last_write, fftime *last_access, fftime *creation)
{
	size_t i;
	FILETIME ft[3];
	fftime *const tt[3] = { creation, last_access, last_write };
	if (0 == GetFileTime(fd, ft + 0, ft + 1, ft + 2))
		return -1;

	for (i = 0; i < 3; i++) {
		if (tt[i] != NULL)
			*tt[i] = _ff_ftToTime(&ft[i]);
	}
	return 0;
}

int fffile_trunc(fffd f, uint64 pos)
{
	int64 curpos = fffile_seek(f, 0, SEEK_CUR);
	if (curpos == -1 || -1 == fffile_seek(f, pos, SEEK_SET))
		return -1;
	if (!SetEndOfFile(f))
		return -1;
	if (-1 == fffile_seek(f, curpos, SEEK_SET))
		return -1;
	return 0;
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

ffdir ffdir_openq(ffsyschar *path, size_t cap, ffdirentry *ent)
{
	ffdir dir;
	size_t len = ffq_len(path);
	ffsyschar *end = path + len;
	if (len + FFSLEN("\\*") >= cap) {
		SetLastError(EOVERFLOW);
		return 0;
	}
	if (!ffpath_slash(path[len - 1]))
		*end++ = FFPATH_SLASH;
	*end++ = '*';
	*end = '\0';
	dir = FindFirstFile(path, &ent->info.data);
	path[len] = '\0';
	ent->info.byH = 0;
	if (dir == INVALID_HANDLE_VALUE)
		return 0;
	ent->namelen = (int)ffq_len(ent->info.data.cFileName);
	ent->next = 0;
	return dir;
}

int ffdir_read(ffdir dir, ffdirentry *ent)
{
	if (ent->next) {
		if (0 == FindNextFile(dir, &ent->info.data))
			return -1;
	}
	else
		ent->next = 1;
	ent->namelen = (int)ffq_len(ent->info.data.cFileName);
	return 0;
}


int fferr_strq(int code, ffsyschar *dst, size_t dst_cap)
{
	int n = FormatMessage(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK
		, 0, code, 0, (LPWSTR)dst, FF_TOINT(dst_cap), 0);
	if (n > 2) {
		n -= FFSLEN(". ");
		dst[n] = L'\0';
	}
	return n;
}


static const uint64 _ff100nsNum = 116444736000000000ULL;

/// Convert Windows FILETIME structure to time value
fftime _ff_ftToTime(const FILETIME *ft)
{
	fftime t = { 0, 0 };
	LARGE_INTEGER li;
	li.LowPart = ft->dwLowDateTime;
	li.HighPart = ft->dwHighDateTime;
	if ((uint64)li.QuadPart > _ff100nsNum)
		fftime_setmcs(&t, (li.QuadPart - _ff100nsNum) / 10);
	return t;
}

/// Convert time value to Windows FILETIME structure
FILETIME _ff_timeToFt(const fftime *time_value)
{
	uint64 d = fftime_mcs(time_value) * 10 + _ff100nsNum;
	FILETIME f;
	f.dwLowDateTime = (uint)d;
	f.dwHighDateTime = (uint)(d >> 32);
	return f;
}

void fftime_now(fftime *t)
{
	FILETIME ft;
	GetSystemTimeAsFileTime(&ft);
	*t = _ff_ftToTime(&ft);
}

void fftime_split(ffdtm *dt, const fftime *t, enum FF_TIMEZONE tz)
{
	SYSTEMTIME st = { 0 };

	if (tz == FFTIME_TZLOCAL) {
		struct tm Tm;
		time_t tt = t->s;
#ifdef FF_MSVC
		localtime_s(&Tm, &tt);
#else
		localtime_r(&tt, &Tm);
#endif
		fftime_fromtm(dt, &Tm);
		dt->msec = t->mcs / 1000;
		return ;
	}

	if (t->s || t->mcs) {
		FILETIME filet = _ff_timeToFt(t);
		FileTimeToSystemTime(&filet, &st);
	}
	else {
		st.wYear = 1970;
		st.wMonth = 1;
		st.wDay = 1;
	}
	dt->year = st.wYear;
	dt->month = st.wMonth;
	dt->weekday = st.wDayOfWeek;
	dt->day = st.wDay;
	dt->hour = st.wHour;
	dt->min = st.wMinute;
	dt->sec = st.wSecond;
	dt->msec = st.wMilliseconds;
}

fftime * fftime_join(fftime *t, const ffdtm *dt, enum FF_TIMEZONE tz)
{
	SYSTEMTIME st = {
		dt->year
		, dt->month
		, 0
		, dt->day
		, dt->hour
		, dt->min
		, dt->sec
		, dt->msec
	};

	if (dt->year < 1970) {
		t->s = t->mcs = 0;
		return t;
	}

	if (tz == FFTIME_TZLOCAL) {
		struct tm Tm;
		time_t tt;
		fftime_totm(&Tm, dt);
		tt = mktime(&Tm);
		t->s = (uint)tt;
		t->mcs = dt->msec * 1000;
		return t;
	}

	SystemTimeToFileTime(&st, (FILETIME *)t);
	*t = _ff_ftToTime((FILETIME *)t);
	return t;
}


size_t ff_utow(WCHAR *dst, size_t dst_cap, const char *src, size_t srclen, int flags)
{
	int r;
	r = MultiByteToWideChar(CP_UTF8, 0/*MB_ERR_INVALID_CHARS*/, src
		, FF_TOINT(srclen), dst, FF_TOINT(dst_cap));
	return r;
}

size_t ff_wtou(char *dst, size_t dst_cap, const WCHAR *src, size_t srclen, int flags)
{
	int r;
	r = WideCharToMultiByte(CP_UTF8, 0, src, FF_TOINT(srclen), dst, FF_TOINT(dst_cap), NULL, NULL);
	return r;
}


int ffthd_join(ffthd th, uint timeout_ms, int *exit_code)
{
	int ret = WaitForSingleObject(th, timeout_ms);
	if (ret == WAIT_OBJECT_0) {
		if (exit_code != NULL)
			(void)GetExitCodeThread(th, (DWORD *)exit_code);
		(void)CloseHandle(th);
		ret = 0;
	}
	else if (ret == WAIT_TIMEOUT)
		ret = ETIMEDOUT;
	return ret;
}

int ffps_wait(fffd h, uint timeout_ms, int *exit_code)
{
	int r = WaitForSingleObject(h, timeout_ms);
	if (r == WAIT_OBJECT_0) {
		if (exit_code != NULL)
			(void)GetExitCodeProcess(h, (DWORD *)exit_code);
		(void)CloseHandle(h);
		r = 0;
	}
	else if (r == WAIT_TIMEOUT)
		r = ETIMEDOUT;
	return r;
}

static FFINL ffsyschar * scopyz(ffsyschar *dst, const ffsyschar *sz) {
	while (*sz != '\0')
		*dst++ = *sz++;
	return dst;
}

fffd ffps_exec(const ffsyschar *filename, const ffsyschar **argv, const ffsyschar **env)
{
	BOOL b;
	PROCESS_INFORMATION info;
	STARTUPINFOW si = { 0 };
	ffsyschar *args
		, *s;
	size_t cap = 0;
	const ffsyschar **a;

	si.cb = sizeof(STARTUPINFOW);

	for (a = argv + 1;  *a != NULL;  a++) {
		cap += ffq_len(*a) + sizeof("\"\" ")-1;
	}

	args = ffmem_alloc((cap + 1) * sizeof(ffsyschar));
	if (args == NULL)
		return FF_BADFD;
	s = args;

	for (a = argv + 1;  *a != NULL;  a++) {
		*s++ = '"';
		s = scopyz(s, *a);
		*s++ = '"';
		*s++ = ' ';
	}
	*s = '\0';

	b = CreateProcess(filename, args, NULL, NULL, 0 /*inherit handles*/, 0, NULL /*env*/
		, NULL /*startup dir*/, &si, &info);

	ffmem_free(args);
	if (!b)
		return FF_BADFD;

	CloseHandle(info.hThread);
	return info.hProcess;
}


LPFN_DISCONNECTEX _ffwsaDisconnectEx;
LPFN_CONNECTEX _ffwsaConnectEx;
LPFN_ACCEPTEX _ffwsaAcceptEx;
LPFN_GETACCEPTEXSOCKADDRS _ffwsaGetAcceptExSockaddrs;

static const void *const _procs[] = {
	(void*)&_ffwsaDisconnectEx, (void*)&_ffwsaConnectEx
	, (void*)&_ffwsaAcceptEx, (void*)&_ffwsaGetAcceptExSockaddrs
};
static const GUID _guids[] = {
	WSAID_DISCONNECTEX, WSAID_CONNECTEX
	, WSAID_ACCEPTEX, WSAID_GETACCEPTEXSOCKADDRS
};

static FFINL int getWsaFunc(ffskt sk, void **func, const GUID *guid)
{
	DWORD b;
	WSAIoctl(sk, SIO_GET_EXTENSION_FUNCTION_POINTER, (void*)guid, sizeof(GUID)
		, func, sizeof(func), &b, 0, 0);
	if (*func == NULL) {
		fferr_set(ERROR_PROC_NOT_FOUND);
		return -1;
	}
	return 0;
}

int _ffwsaGetFuncs()
{
	int rc = 0;
	size_t i;
	ffskt sk;

	if (_ffwsaGetAcceptExSockaddrs != NULL)
		return 0;

	sk = ffskt_create(AF_INET, SOCK_STREAM, 0);
	if (sk == FF_BADSKT)
		return -1;
	for (i = 0;  i < FFCNT(_guids);  i++) {
		if (0 != getWsaFunc(sk, (void **)_procs[i], &_guids[i])) {
			rc = -1;
			break;
		}
	}
	ffskt_close(sk);
	return rc;
}


int ffaio_acceptbegin(ffaio_acceptor *acc, ffaio_handler handler)
{
	ffaio_task *t = &acc->task;
	ffskt sk;
	BOOL b;
	DWORD r;

	sk = ffskt_create(acc->family, acc->sktype, 0);
	if (sk == FF_BADSKT)
		return FFAIO_ERROR;

	ffmem_tzero(&t->rovl);
	b = _ffwsaAcceptEx(t->sk, sk, acc->addrs, 0, FFADDR_MAXLEN + 16, FFADDR_MAXLEN + 16, &r, &t->rovl);
	if (0 != fferr_ioret(b)) {
		int er = fferr_last();
		(void)ffskt_close(sk);
		fferr_set(er);
		return FFAIO_ERROR;
	}

	acc->csk = sk;
	t->rhandler = handler;
	return FFAIO_ASYNC;
}

static FFINL void getNAddrs(byte *addrs, ffaddr *local, ffaddr *peer)
{
	int lenP = 0
		, lenL = 0;
	struct sockaddr *lo
		, *pe;
	_ffwsaGetAcceptExSockaddrs(addrs, 0, FFADDR_MAXLEN + 16, FFADDR_MAXLEN + 16, &lo, &lenL, &pe, &lenP);

	memcpy(&local->a, lo, lenL);
	local->len = lenL;
	memcpy(&peer->a, pe, lenP);
	peer->len = lenP;
}

ffskt ffaio_accept(ffaio_acceptor *acc, ffaddr *local, ffaddr *peer, int flags)
{
	ffaio_task *t = &acc->task;
	ffskt sk = FF_BADSKT;
	int er = 0;

	if (acc->csk == FF_BADSKT) {
		peer->len = FFADDR_MAXLEN;
		sk = ffskt_accept(t->sk, &peer->a, &peer->len, flags);
		if (sk != FF_BADSKT)
			(void)ffskt_local(sk, local);
		return sk;
	}

	if (0 == ffio_result(&t->rovl)) {
		sk = acc->csk;
		if (flags & SOCK_NONBLOCK)
			(void)ffskt_nblock(sk, 1);

		getNAddrs(acc->addrs, local, peer);

	} else {
		er = fferr_last();
		(void)ffskt_close(acc->csk);
	}

	acc->csk = FF_BADSKT;
	ffmem_zero(acc->addrs, sizeof(acc->addrs));

	if (er != 0)
		fferr_set(er);
	return sk;
}

int ffaio_connect(ffaio_task *t, ffaio_handler handler, const struct sockaddr *addr, socklen_t addr_size)
{
	BOOL b;
	DWORD r;
	ffaddr a;
	ffaddr_init(&a);
	ffaddr_setany(&a, addr->sa_family);
	if (0 != ffskt_bind(t->sk, &a.a, a.len))
		goto fail;

	ffmem_tzero(&t->wovl);
	b = _ffwsaConnectEx(t->sk, addr, addr_size, NULL, 0, &r, &t->wovl);

	if (0 != fferr_ioret(b))
		goto fail;

	t->whandler = handler;
	return FFAIO_ASYNC;

fail:
	t->result = -1;
	return FFAIO_ERROR;
}

int ffaio_recv(ffaio_task *t, ffaio_handler handler, void *d, size_t cap)
{
	BOOL b;
	DWORD rd;

	if (t->rsig && cap == 0) {
		/* Reuse the previous 'read' operation.
		There's no need to cancel the previous and start a new one. */
		t->rhandler = handler;
		return FFAIO_ASYNC;
	}

	ffmem_tzero(&t->rovl);
	b = ReadFile(t->fd, d, FF_TOINT(cap), &rd, &t->rovl);

	if (0 != fferr_ioret(b)) {
		t->result = -1;
		return FFAIO_ERROR;
	}

	t->rhandler = handler;
	if (cap == 0)
		t->rsig = 1;
	return FFAIO_ASYNC;
}

int ffaio_send(ffaio_task *t, ffaio_handler handler, const void *d, size_t len)
{
	BOOL b;
	DWORD wr;

	ffmem_tzero(&t->wovl);
	b = WriteFile(t->fd, d, (int)ffmin(len, 1), &wr, &t->wovl);

	if (0 != fferr_ioret(b)) {
		t->result = -1;
		return FFAIO_ERROR;
	}

	t->whandler = handler;
	return FFAIO_ASYNC;
}

int ffaio_cancelasync(ffaio_task *t, int op, ffaio_handler oncancel)
{
	if ((op & FFAIO_READ) && t->rhandler != NULL) {
		if (oncancel != NULL)
			t->rhandler = oncancel;
		CancelIoEx(t->fd, &t->rovl);
	}

	if ((op & FFAIO_WRITE) && t->whandler != NULL) {
		if (oncancel != NULL)
			t->whandler = oncancel;
		CancelIoEx(t->fd, &t->wovl);
	}

	t->canceled = 1;
	return 0;
}

int _ffaio_events(ffaio_task *t, const ffkqu_entry *e)
{
	int ev = 0;

	if (e->lpOverlapped != NULL) {
		t->result = ffio_result(e->lpOverlapped);
		if (t->result == -1)
			ev |= FFKQU_ERR;
	}

	if (e->lpOverlapped == &t->wovl)
		ev |= FFKQU_WRITE;
	else {
		if (t->rsig)
			t->rsig = 0;

		ev |= FFKQU_READ;
	}

	if (t->canceled) {
		t->result = -1;
		ev |= FFKQU_ERR;
		fferr_set(ECANCELED); //overwrite errno even if operation has completed successfully

		if (((ev & FFKQU_WRITE) && t->rhandler == NULL)
			|| ((ev & FFKQU_READ) && t->whandler == NULL))
			t->canceled = 0; //reset flag only if both handlers have signaled
	}

	return ev;
}


void ffclk_diff(const fftime *start, fftime *diff)
{
	LARGE_INTEGER freq
		, first
		, second;

	if (!QueryPerformanceFrequency(&freq)) {
		diff->s = diff->mcs = 0;
		return ;
	}

	first.LowPart = start->s;
	first.HighPart = start->mcs;
	second.LowPart = diff->s;
	second.HighPart = diff->mcs;
	fftime_setns(diff, ((second.QuadPart - first.QuadPart) * 1000000000) / freq.QuadPart);
}


fftmr fftmr_create(int flags)
{
	fftmr tmr = ffmem_alloc(sizeof(fftmr_s));
	if (tmr == NULL)
		return FF_BADTMR;

	tmr->htmr = CreateWaitableTimerEx(NULL, NULL, flags /*CREATE_WAITABLE_TIMER_MANUAL_RESET*/, TIMER_ALL_ACCESS);
	if (tmr->htmr == NULL) {
		ffmem_free(tmr);
		return FF_BADTMR;
	}
	return tmr;
}

void __stdcall _fftmr_onfire(LPVOID arg, DWORD dwTimerLowValue, DWORD dwTimerHighValue)
{
	fftmr t = arg;
	ffkqu_post(t->kq, t->data, NULL);
}

int fftmr_start(fftmr tmr, fffd qu, void *data, int periodMs)
{
	uint period = periodMs;
	int64 due_ns100 = (int64)periodMs * 1000 * -10;
	if (periodMs < 0) {
		period = 0;
		due_ns100 = -due_ns100;
	}
	tmr->kq = qu;
	tmr->data = data;
	return 0 == SetWaitableTimer(tmr->htmr, (LARGE_INTEGER*)&due_ns100, period, &_fftmr_onfire, tmr, 1);
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

int ffsig_ctl(ffaio_task *t, fffd kq, const int *sigs, size_t nsigs, ffaio_handler handler)
{
	ffsyschar name[64];
	BOOL b;

	if (handler == NULL) {
		FF_SAFECLOSE(t->fd, FF_BADFD, (void)ffpipe_close);
		return 0;
	}

	pipename(name, FFCNT(name), ffps_curid());
	t->fd = ffpipe_createnamed(name);
	if (t->fd == FF_BADFD)
		return 1;

	if (0 != ffaio_attach(t, kq, FFKQU_READ))
		return 1;

	ffmem_tzero(&t->wovl);
	b = ConnectNamedPipe(t->fd, &t->wovl);
	if (0 != fferr_ioret(b))
		return -1;

	t->whandler = handler;
	t->oneshot = 0;
	return 0;
}

int ffsig_read(ffaio_task *t)
{
	ssize_t r;
	byte b;

	r = fffile_read(t->fd, &b, 1);
	if (r == 1)
		return b;

	ffpipe_disconnect(t->fd);
	ffmem_tzero(&t->wovl);
	b = ConnectNamedPipe(t->fd, &t->wovl);
	// if (0 != fferr_ioret(b))
		// return -1;
	return -1;
}

int ffps_sig(int pid, int sig)
{
	ffsyschar name[64];
	fffd p;
	byte b;

	pipename(name, FFCNT(name), pid);

	p = fffile_openq(name, FFO_OPEN | O_WRONLY);
	if (p == FF_BADFD)
		return 1;

	b = (byte)sig;
	if (1 != fffile_write(p, &b, 1)) {
		(void)ffpipe_close(p);
		return 1;
	}

	(void)ffpipe_close(p);
	return 0;
}

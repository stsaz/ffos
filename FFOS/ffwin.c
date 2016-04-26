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
#include <FFOS/sig.h>

#include <time.h>
#ifndef FF_MSVC
#include <stdio.h>
#endif


HANDLE _ffheap;

/* [allocated-start] ... [pointer to allocated-start] [aligned] ... [allocated-end] */
void* _ffmem_align(size_t size, size_t align)
{
	void *buf, *al;

	if ((align % sizeof(void*)) != 0) {
		fferr_set(EINVAL);
		return NULL;
	}

	buf = _ffmem_alloc(size + align + sizeof(void*));
	if (buf == NULL)
		return NULL;

	al = (void*)ff_align((size_t)buf + sizeof(void*), align);
	*((void**)al - 1) = buf; //remember the original pointer
	return al;
}


char* ffenv_expand(char *dst, size_t cap, const char *src)
{
	ffsyschar *wsrc, *wdst = NULL;
	size_t wlen;

	if (NULL == (wsrc = ffs_utow(NULL, NULL, src, -1)))
		return NULL;

	wlen = ExpandEnvironmentStrings(wsrc, NULL, 0);
	if (NULL == (wdst = ffq_alloc(wlen)))
		goto done;
	ExpandEnvironmentStrings(wsrc, wdst, wlen);

	if (dst == NULL) {
		cap = ff_wtou(dst, cap, wdst, wlen + 1, 0);
		if (NULL == (dst = ffmem_alloc(cap)))
			goto done;
	}
	ff_wtou(dst, cap, wdst, wlen + 1, 0);

done:
	ffmem_free(wsrc);
	ffmem_safefree(wdst);
	return dst;
}

int ffenv_update(void)
{
	DWORD_PTR r;
	return !SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"Environment", SMTO_ABORTIFHUNG , 5000, &r);
}


fffd fffile_openq(const ffsyschar *filename, int flags)
{
	enum { share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE };
	int mode = (flags & 0x0000000f);
	int access = (flags & 0x000000f0) << 24;
	int f = (flags & 0xffff0000) | FILE_ATTRIBUTE_NORMAL
		| FILE_FLAG_BACKUP_SEMANTICS /*open directories*/;

	if ((flags & FFO_NODOSNAME) && !ffpath_islong(filename)) {
		return FF_BADFD;
	}

	if (mode == 0)
		mode = OPEN_EXISTING;
	else if (mode == FFO_APPEND) {
		mode = OPEN_ALWAYS;
		access = STANDARD_RIGHTS_WRITE | FILE_APPEND_DATA | SYNCHRONIZE;
	}

	if (flags & O_TRUNC) {
		mode = (mode == OPEN_ALWAYS) ? CREATE_ALWAYS
			: (mode == OPEN_EXISTING) ? TRUNCATE_EXISTING
			: 0;
	}

	return CreateFile(filename, access, share, NULL, mode, f, NULL);
}

fffd fffile_open(const char *filename, int flags)
{
	fffd f;
	ffsyschar *w, ws[FF_MAXFN];
	size_t n = FFCNT(ws);
	if (NULL == (w = ffs_utow(ws, &n, filename, -1)))
		return FF_BADFD;

	f = fffile_openq(ws, flags);
	if (w != ws)
		ffmem_free(w);
	return f;
}

int fffile_trunc(fffd f, uint64 pos)
{
	int r = 0;
	int64 curpos = fffile_seek(f, 0, SEEK_CUR);
	if (curpos == -1 || -1 == fffile_seek(f, pos, SEEK_SET))
		return -1;
	if (!SetEndOfFile(f))
		r = -1;
	if (-1 == fffile_seek(f, curpos, SEEK_SET))
		r = -1;
	return r;
}

int fffile_infofn(const char *fn, fffileinfo *fi)
{
	ffsyschar *w, wname[FF_MAXFN];
	WIN32_FILE_ATTRIBUTE_DATA fad;
	size_t n = FFCNT(wname);
	BOOL b;

	if (NULL == (w = ffs_utow(wname, &n, fn, -1)))
		return -1;
	b = GetFileAttributesEx(w, GetFileExInfoStandard, &fad);
	if (w != wname)
		ffmem_free(w);
	if (!b)
		return -1;

	fi->dwFileAttributes = fad.dwFileAttributes;
	fi->ftCreationTime = fad.ftCreationTime;
	fi->ftLastAccessTime = fad.ftLastAccessTime;
	fi->ftLastWriteTime = fad.ftLastWriteTime;
	fi->nFileSizeHigh = fad.nFileSizeHigh;
	fi->nFileSizeLow = fad.nFileSizeLow;
	fi->nFileIndexHigh = fi->nFileIndexLow = 0;
	return 0;
}

int fffile_rename(const char *src, const char *dst)
{
	ffsyschar wsrc[FF_MAXFN], wdst[FF_MAXFN], *ws = wsrc, *wd = wdst;
	size_t ns = FFCNT(wsrc), nd = FFCNT(wdst);
	int r = -1;

	if (NULL == (ws = ffs_utow(wsrc, &ns, src, -1))
		|| NULL == (wd = ffs_utow(wdst, &nd, dst, -1)))
		goto fail;

	r = fffile_renameq(ws, wd);

fail:
	if (ws != wsrc)
		ffmem_free(ws);
	if (wd != wdst)
		ffmem_free(wd);
	return r;
}

int fffile_rm(const char *name)
{
	ffsyschar ws[FF_MAXFN], *w;
	size_t n = FFCNT(ws);
	int r;

	if (NULL == (w = ffs_utow(ws, &n, name, -1)))
		return -1;
	r = fffile_rmq(ws);
	if (w != ws)
		ffmem_free(w);
	return r;
}


ssize_t ffstd_write(fffd h, const char *s, size_t len)
{
	wchar_t ws[1024];
	size_t r = FFCNT(ws);
	DWORD wr;
	if (GetConsoleMode(h, &wr)) {
		wchar_t *w = ffs_utow(ws, &r, s, len);
		BOOL b = WriteConsole(h, w, FF_TOINT(r), &wr, NULL);
		if (w != ws)
			ffmem_free(w);
		return b ? (ssize_t)len : -1;
	}
	return fffile_write(h, s, len);
}

int ffstd_attr(fffd fd, uint attr, uint val)
{
	DWORD mode;
	if (!GetConsoleMode(fd, &mode))
		return -1;

	if (attr & FFSTD_ECHO) {
		if (val & FFSTD_ECHO)
			mode |= ENABLE_ECHO_INPUT;
		else
			mode &= ~ENABLE_ECHO_INPUT;
	}

	if (attr & FFSTD_LINEINPUT) {
		if (val & FFSTD_LINEINPUT)
			mode |= ENABLE_LINE_INPUT;
		else
			mode &= ~ENABLE_LINE_INPUT;
	}

	SetConsoleMode(fd, mode);
	return 0;
}

// VK_* -> FFKEY_*
static const byte vkeys[] = {
	VK_LEFT, VK_UP, VK_RIGHT, VK_DOWN,
};
static const byte ffkeys[] = {
	(byte)FFKEY_LEFT, (byte)FFKEY_UP, (byte)FFKEY_RIGHT, (byte)FFKEY_DOWN,
};

static ssize_t ffint_binfind1(const byte *arr, size_t n, uint search)
{
	size_t i, start = 0;
	while (start != n) {
		i = start + (n - start) / 2;
		if (search == arr[i])
			return i;
		else if (search < arr[i])
			n = i;
		else
			start = i + 1;
	}
	return -1;
}

int ffstd_key(const char *data, size_t *len)
{
	const KEY_EVENT_RECORD *k = (void*)data;
	uint r, ctl = k->dwControlKeyState;

	if (k->uChar.AsciiChar != 0)
		r = k->uChar.AsciiChar;
	else {
		if ((uint)-1 == (r = ffint_binfind1(vkeys, FFCNT(vkeys), k->wVirtualKeyCode)))
			return -1;
		r = FFKEY_VIRT | ffkeys[r];
	}

	if (ctl & (RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED))
		r |= FFKEY_ALT;
	if (ctl & (RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED))
		r |= FFKEY_CTRL;
	if (ctl & SHIFT_PRESSED)
		r |= FFKEY_SHIFT;

	return r;
}

int ffstd_event(fffd fd, ffstd_ev *ev)
{
	DWORD n;
	int r;
	enum { I_READ, I_NEXT };

	for (;;) {
	switch (ev->state) {

	case I_READ:
		r = WaitForSingleObject(fd, INFINITE);
		if (r != WAIT_OBJECT_0)
			return -1;

		if (!GetNumberOfConsoleInputEvents(fd, &n))
			return -1;
		if (n == 0)
			return 0;

		if (!ReadConsoleInput(fd, ev->rec, ffmin(n, FFCNT(ev->rec)), &n))
			return -1;
		if (n == 0)
			return 0;
		ev->nrec = n;
		ev->irec = 0;
		ev->state = I_NEXT;
		// break

	case I_NEXT:
		if (ev->irec == ev->nrec) {
			ev->state = I_READ;
			continue;
		}
		if (!(ev->rec[ev->irec].EventType == KEY_EVENT && ev->rec[ev->irec].Event.KeyEvent.bKeyDown)) {
			ev->irec++;
			continue;
		}
		ev->data = (void*)&ev->rec[ev->irec].Event.KeyEvent;
		ev->datalen = sizeof(ev->rec[ev->irec].Event.KeyEvent);
		ev->irec++;
		return 1;
	}
	}
}


fffd ffpipe_create_named(const char *name)
{
	fffd f;
	ffsyschar *w, ws[FF_MAXFN];
	size_t n = FFCNT(ws);
	if (NULL == (w = ffs_utow(ws, &n, name, -1)))
		return FF_BADFD;

	f = ffpipe_create_namedq(ws);
	if (w != ws)
		ffmem_free(w);
	return f;
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


int ffdir_make(const char *name)
{
	ffsyschar ws[FF_MAXFN], *w;
	size_t n = FFCNT(ws);
	int r;

	if (NULL == (w = ffs_utow(ws, &n, name, -1)))
		return -1;
	r = ffdir_makeq(w);
	if (w != ws)
		ffmem_free(w);
	return r;
}

int ffdir_rmake(const char *path, size_t off)
{
	ffsyschar ws[FF_MAXFN], *w;
	size_t n = FFCNT(ws);
	int r;

	if (NULL == (w = ffs_utow(ws, &n, path, -1)))
		return -1;

	if (off == 0 && ffpath_abs(path, strlen(path)))
		off = FFSLEN("c:"); // don't try to make directory "drive:"

	r = ffdir_rmakeq(w, off);
	if (w != ws)
		ffmem_free(w);
	return r;
}

int ffdir_rm(const char *name)
{
	ffsyschar ws[FF_MAXFN], *w;
	size_t n = FFCNT(ws);
	int r;

	if (NULL == (w = ffs_utow(ws, &n, name, -1)))
		return -1;
	r = ffdir_rmq(w);
	if (w != ws)
		ffmem_free(w);
	return r;
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
	dir = FindFirstFileEx(path, FindExInfoBasic, &ent->info, 0, NULL, 0);
	path[len] = '\0';
	if (dir == INVALID_HANDLE_VALUE)
		return 0;
	ent->next = 0;
	return dir;
}

ffdir ffdir_open(char *path, size_t cap, ffdirentry *ent)
{
	ffdir d;
	ffsyschar *w, ws[FF_MAXFN];
	size_t n;

	n = ff_utow(NULL, 0, path, -1, 0) + FFSLEN("/*") + 1;

	w = ws;
	if (n > FFCNT(ws)
		&& NULL == (w = ffq_alloc(n)))
		return NULL;

	ff_utow(w, n, path, -1, 0);

	d = ffdir_openq(w, FF_MAXPATH, ent);
	if (w != ws)
		ffmem_free(w);
	return d;
}

int ffdir_read(ffdir dir, ffdirentry *ent)
{
	if (ent->next) {
		if (0 == FindNextFile(dir, &ent->info))
			return -1;
	}
	else
		ent->next = 1;
	ent->namelen = (int)ffq_len(ent->info.cFileName);
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
	return 0;
}

const char* fferr_strp(int code)
{
	static char se[255];
	fferr_str(code, se, sizeof(se));
	return se;
}


static const uint64 _ff100nsNum = 116444736000000000ULL;

/// Convert Windows FILETIME structure to time value
fftime _ff_ftToTime(const FILETIME *ft)
{
	fftime t = { 0, 0 };
	uint64 i = ((uint64)ft->dwHighDateTime << 32) | ft->dwLowDateTime;
	if (i > _ff100nsNum)
		fftime_setmcs(&t, (i - _ff100nsNum) / 10);
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
#if defined FF_MSVC || defined FF_MINGW
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

	if (tz == FFTIME_TZNODATE) {
		t->s = dt->hour * 60*60 + dt->min * 60 + dt->sec;
		t->mcs = dt->msec * 1000;
		return t;
	}

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
	dst = ffmem_talloc(WCHAR, wlen);
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

#ifdef FF_USE_SYS_WTOU
size_t ff_wtou(char *dst, size_t dst_cap, const WCHAR *src, size_t srclen, int flags)
{
	int r;
	r = WideCharToMultiByte(CP_UTF8, 0, src, FF_TOINT(srclen), dst, FF_TOINT(dst_cap), NULL, NULL);
	return r;
}

#else
FF_EXTN size_t ffutf8_encode(char *dst, size_t cap, const char *src, size_t *len, uint flags);
size_t ff_wtou(char *dst, size_t cap, const wchar_t *src, size_t len, int flags)
{
	enum {
		FFU_UTF16LE = 1,
		FFU_FWHOLE = 1 << 31
	};
	len = (len != (size_t)-1) ? 2 * len : 2 * (ffq_len(src) + 1);
	size_t ln = len;
	size_t r = ffutf8_encode(dst, cap, (char*)src, &ln, FFU_UTF16LE | FFU_FWHOLE);
	if (ln != len)
		return 0; //not enough output space
	return r;
}
#endif


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
		fferr_set(ETIMEDOUT);
	return r;
}

static FFINL ffsyschar * scopyz(ffsyschar *dst, const ffsyschar *sz) {
	while (*sz != '\0')
		*dst++ = *sz++;
	return dst;
}

fffd ffps_exec(const char *filename, const char **argv, const char **env)
{
	BOOL b;
	PROCESS_INFORMATION info;
	STARTUPINFOW si = { 0 };
	ffsyschar wfn_s[255], *wfn, *args
		, *s;
	ffbool has_space;
	size_t cap = 0, i;

	si.cb = sizeof(STARTUPINFOW);

	cap = FFCNT(wfn_s);
	if (NULL == (wfn = ffs_utow(wfn_s, &cap, filename, -1)))
		return FF_BADFD;

	cap += FFSLEN("\"\" ");
	for (i = 0;  argv[i] != NULL;  i++) {
		cap += ff_utow(NULL, 0, argv[i], -1, 0) + FFSLEN("\"\" ");
	}

	args = ffmem_alloc((cap + 1) * sizeof(ffsyschar));
	if (args == NULL) {
		if (wfn != wfn_s)
			ffmem_free(wfn);
		return FF_BADFD;
	}
	s = args;

	has_space = (NULL != strchr(filename, ' '));
	if (has_space)
		*s++ = '"';
	s = scopyz(s, wfn);
	if (has_space)
		*s++ = '"';
	*s++ = ' ';

	for (i = 0;  argv[i] != NULL;  i++) {
		has_space = (NULL != strchr(argv[i], ' '));
		if (has_space)
			*s++ = '"';
		ff_utow(s, args + cap - s, argv[i], -1, 0);
		s += ffq_len(s);
		if (has_space)
			*s++ = '"';
		*s++ = ' ';
	}
	*s = '\0';

	b = CreateProcess(wfn, args, NULL, NULL, 0 /*inherit handles*/, 0, NULL /*env*/
		, NULL /*startup dir*/, &si, &info);

	if (wfn != wfn_s)
		ffmem_free(wfn);
	ffmem_free(args);
	if (!b)
		return FF_BADFD;

	CloseHandle(info.hThread);
	return info.hProcess;
}


fffd ffdl_open(const char *filename, int flags)
{
	fffd f;
	ffsyschar *w, ws[FF_MAXFN];
	size_t n = FFCNT(ws);
	if (NULL == (w = ffs_utow(ws, &n, filename, -1)))
		return FF_BADFD;

	f = ffdl_openq(ws, flags);
	if (w != ws)
		ffmem_free(w);
	return f;
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


static int _ffaio_acceptbegin(ffaio_acceptor *acc)
{
	ffskt sk;
	BOOL b;
	DWORD r;

	sk = ffskt_create(acc->family, acc->sktype, 0);
	if (sk == FF_BADSKT)
		return FFAIO_ERROR;

	ffmem_tzero(&acc->kev.ovl);
	b = _ffwsaAcceptEx(acc->kev.sk, sk, acc->addrs, 0, FFADDR_MAXLEN + 16, FFADDR_MAXLEN + 16, &r, &acc->kev.ovl);
	if (0 != fferr_ioret(b)) {
		int er = fferr_last();
		(void)ffskt_close(sk);
		fferr_set(er);
		return FFAIO_ERROR;
	}

	acc->csk = sk;
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

ffskt ffaio_accept(ffaio_acceptor *acc, ffaddr *local, ffaddr *peer, int flags, ffkev_handler handler)
{
	ffskt sk = FF_BADSKT;
	int er = 0;

	if (acc->csk == FF_BADSKT) {
		peer->len = FFADDR_MAXLEN;
		sk = ffskt_accept(acc->kev.sk, &peer->a, &peer->len, flags);
		if (sk != FF_BADSKT)
			(void)ffskt_local(sk, local);

		else if (fferr_again(fferr_last()) && FFAIO_ASYNC == _ffaio_acceptbegin(acc)) {
			acc->kev.handler = handler;
			fferr_set(WSAEWOULDBLOCK);
		}

		return sk;
	}

	if (0 == ffio_result(&acc->kev.ovl)) {
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
		return ffio_result(&ft->kev.ovl);
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

	ft->kev.pending = 1;
	ft->kev.handler = handler;
	fferr_set(EAGAIN);
	return -1;
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
	fftmr tmr = ffmem_tcalloc1(fftmr_s);
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

static int __stdcall _fftmr_thd(void *param)
{
	fftmr_s *tmr = param;
	uint period = tmr->period;
	int64 due_ns100 = (int64)tmr->period * 1000 * -10;
	if (tmr->period < 0) {
		period = 0;
		due_ns100 = -due_ns100;
	}
	SetWaitableTimer(tmr->htmr, (LARGE_INTEGER*)&due_ns100, period, &_fftmr_onfire, tmr, 1);

	for (;;) {
		SleepEx(INFINITE, 1 /*alertable*/);
	}
	return 0;
}

int fftmr_start(fftmr tmr, fffd qu, void *data, int periodMs)
{
	tmr->kq = qu;
	tmr->data = data;
	tmr->period = periodMs;

	FF_ASSERT(tmr->thd == NULL);
	if (NULL == (tmr->thd = ffthd_create(&_fftmr_thd, tmr, 0)))
		return FF_BADTMR;
	return 0;
}

int fftmr_stop(fftmr tmr, fffd kq)
{
	TerminateThread(tmr->thd, -1);
	ffthd_detach(tmr->thd);
	tmr->thd = NULL;

	return 0 == CancelWaitableTimer(tmr->htmr);
}

int fftmr_close(fftmr tmr, fffd kq)
{
	int r = (0 == CloseHandle(tmr->htmr));

	if (tmr->thd != NULL) {
		TerminateThread(tmr->thd, -1);
		ffthd_detach(tmr->thd);
	}

	ffmem_free(tmr);
	return r;
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

		if (t->fd == FF_BADFD)
			ffpipe_close(t->fd);
		ffkev_fin(t);
		return 0;
	}

	pipename(name, FFCNT(name), ffps_curid());
	t->fd = ffpipe_create_namedq(name);
	if (t->fd == FF_BADFD)
		return 1;

	if (0 != ffkqu_attach(kq, t->fd, ffkev_ptr(t), 0))
		return 1;

	if (0 != ffaio_pipe_listen(t, handler))
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

	r = fffile_read(t->fd, &b, 1);
	if (r == 1)
		return b;

	ffpipe_disconnect(t->fd);
	if (0 == ffaio_pipe_listen(t, t->handler)) {
		fferr_set(EAGAIN);
		return -1;
	}
	return -1;
}

int ffps_sig(int pid, int sig)
{
	ffsyschar name[64];
	fffd p;
	byte b;

	pipename(name, FFCNT(name), pid);

	p = fffile_openq(name, O_WRONLY);
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

const char* ffps_filename(char *name, size_t cap, const char *argv0)
{
	ffsyschar fnw[FF_MAXPATH];
	size_t n = GetModuleFileName(NULL, fnw, FFCNT(fnw));
	n = ff_wtou(name, cap, fnw, n, 0);
	if (n == 0 || n == cap)
		return NULL;
	name[n] = '\0';
	return name;
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


typedef HKEY ffwreg;

#define FFWREG_BADKEY  ((HKEY)-1)

ffwreg ffwreg_open(HKEY hk, const char *path, uint flags)
{
	ffsyschar ws[255], *w;
	ffwreg k = FFWREG_BADKEY;
	size_t n = FFCNT(ws);
	if (NULL == (w = ffs_utow(ws, &n, path, -1)))
		return FFWREG_BADKEY;

	RegCreateKeyExW(hk, w, 0, NULL, 0, flags, NULL, &k, NULL);

	if (w != ws)
		ffmem_free(w);
	return k;
}

int ffwreg_readstr(ffwreg k, const char *name, char *d, size_t *cap)
{
	int r = -1;
	ffsyschar ws[255], *w;
	ffsyschar *wval = NULL;
	DWORD t, size;
	size_t n = FFCNT(ws);
	if (NULL == (w = ffs_utow(ws, &n, name, -1)))
		return -1;

	for (;;) {
		t = REG_SZ;
		size = 0;
		if (RegQueryValueExW(k, w, 0, &t, NULL, &size))
			goto done;

		if ((size - 1) / sizeof(ffsyschar) * 4 > *cap) {
			*cap = (size - 1) / sizeof(ffsyschar) * 4;
			fferr_set(ERROR_MORE_DATA);
			goto done;
		}

		if (NULL == (wval = ffmem_alloc(size)))
			goto done;

		t = REG_SZ;
		if (RegQueryValueExW(k, w, 0, &t, (void*)wval, &size)) {
			if (fferr_last() == ERROR_MORE_DATA) {
				ffmem_free(wval);
				wval = NULL;
				continue;
			}
			goto done;
		}

		break;
	}

	if (!(t == REG_SZ || t == REG_EXPAND_SZ)) {
		fferr_set(EINVAL);
		goto done;
	}

	if (size != 0)
		size = (size - 1) / sizeof(ffsyschar);

	r = ff_wtou(d, *cap, wval, size, 0);

done:
	ffmem_safefree(wval);
	if (w != ws)
		ffmem_free(w);
	return r;
}

int ffwreg_writestr(ffwreg k, const char *name, const char *val, size_t len)
{
	int r = -1;
	ffsyschar ws[255], *w;
	ffsyschar ws_val[255], *wval;
	size_t n = FFCNT(ws);
	if (NULL == (w = ffs_utow(ws, &n, name, -1)))
		return -1;

	n = FFCNT(ws_val);
	if (NULL == (wval = ffs_utow(ws_val, &n, val, len)))
		goto done;
	wval[n++] = '\0';

	uint type = REG_SZ;
	if (!RegSetValueExW(k, w, 0, type, (void*)wval, n * sizeof(ffsyschar)))
		r = 0;

done:
	if (wval != ws_val)
		ffmem_safefree(wval);
	if (w != ws)
		ffmem_safefree(w);
	return r;
}

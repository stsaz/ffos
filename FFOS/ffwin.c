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

#include <psapi.h>
#include <time.h>
#ifndef FF_MSVC
#include <stdio.h>
#endif
#include <winternl.h>
#include <ntstatus.h>


HANDLE _ffheap;

static ffps ffps_exec_cmdln_q2(const ffsyschar *filename, ffsyschar *cmdln, ffps_execinfo *i);


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

	al = (void*)ff_align_ceil((size_t)buf + sizeof(void*), align);
	*((void**)al - 1) = buf; //remember the original pointer
	return al;
}


char* ffenv_expand(void *unused, char *dst, size_t cap, const char *src)
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
		cap = ff_wtou(dst, cap, wdst, wlen, 0);
		if (NULL == (dst = ffmem_alloc(cap)))
			goto done;
	}
	ff_wtou(dst, cap, wdst, wlen, 0);

done:
	ffmem_free(wsrc);
	ffmem_safefree(wdst);
	return dst;
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

	if (flags & FFO_TRUNC) {
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

	f = fffile_openq(w, flags);
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

int fffile_attrsetfn(const char *fn, uint attr)
{
	int r;
	ffsyschar *w, ws[FF_MAXFN];
	size_t n = FFCNT(ws);
	if (NULL == (w = ffs_utow(ws, &n, fn, -1)))
		return -1;

	r = fffile_attrsetfnq(w, attr);
	if (w != ws)
		ffmem_free(w);
	return r;
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

int fffile_hardlink(const char *target, const char *linkname)
{
	ffsyschar wsrc_s[FF_MAXFN], *wsrc = wsrc_s;
	ffsyschar wdst_s[FF_MAXFN], *wdst = wdst_s;
	size_t ns = FFCNT(wsrc_s), nd = FFCNT(wdst_s);
	int r = -1;

	if (NULL == (wsrc = ffs_utow(wsrc_s, &ns, target, -1))
		|| NULL == (wdst = ffs_utow(wdst_s, &nd, linkname, -1)))
		goto fail;

	r = fffile_hardlinkq(wsrc, wdst);

fail:
	if (wsrc != wsrc_s)
		ffmem_free(wsrc);
	if (wdst != wdst_s)
		ffmem_free(wdst);
	return r;
}

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

int fffile_rmq(const ffsyschar *name)
{
	if (!!DeleteFile(name))
		return 0;

	if (fferr_last() == EACCES) {
		int attr = GetFileAttributes(name);
		if (attr == -1
			|| !(attr & FFWIN_FILE_READONLY)
			|| !!fffile_attrsetfnq(name, attr & ~FFWIN_FILE_READONLY))
			return 1;
		if (!!DeleteFile(name))
			return 0;
	}

	return 1;
}

int fffile_rm(const char *name)
{
	ffsyschar ws[FF_MAXFN], *w;
	size_t n = FFCNT(ws);
	int r;

	if (NULL == (w = ffs_utow(ws, &n, name, -1)))
		return -1;
	r = fffile_rmq(w);
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
	enum { I_READ, I_NEXT };

	for (;;) {
	switch (ev->state) {

	case I_READ:
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
		// fall through

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


ffsem ffsem_openq(const ffsyschar *name, uint flags, uint value)
{
	if (name == NULL)
		return CreateSemaphore(NULL, value, 0xffff, NULL);

	if (flags == FFO_CREATENEW) {
		ffsem s = OpenSemaphore(SEMAPHORE_ALL_ACCESS, 0, name);
		if (s != FFSEM_INV) {
			CloseHandle(s);
			fferr_set(EEXIST);
			return FFSEM_INV;
		}
		return CreateSemaphore(NULL, value, 0xffff, name);

	} else if (flags == FFO_CREATE)
		return CreateSemaphore(NULL, value, 0xffff, name);

	else if (flags == 0)
		return OpenSemaphore(SEMAPHORE_ALL_ACCESS, 0, name);

	fferr_set(EINVAL);
	return FFSEM_INV;
}

ffsem ffsem_open(const char *name, uint flags, uint value)
{
	ffsyschar *w = NULL, ws[FF_MAXFN];
	size_t n = FFCNT(ws);
	if (name != NULL
		&& NULL == (w = ffs_utow(ws, &n, name, -1)))
		return FFSEM_INV;

	ffsem s = ffsem_openq(w, flags, value);
	if (w != ws)
		ffmem_free(w);
	return s;
}

int ffsem_wait(ffsem s, uint time_ms)
{
	int r = WaitForSingleObject(s, time_ms);
	if (r == WAIT_OBJECT_0)
		r = 0;
	else if (r == WAIT_TIMEOUT)
		fferr_set(ETIMEDOUT);
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

	if (off == 0 && !ffpath_slash(path[0]) && ffpath_abs(path, strlen(path)))
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
#if FF_WIN >= 0x0600
	dir = FindFirstFileEx(path, FindExInfoBasic, &ent->info, 0, NULL, 0);
#else
	dir = FindFirstFile(path, &ent->info);
#endif
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
	if (n == 0) {
		if (dst_cap != 0)
			dst[0] = '\0';
		return -1;
	}
	if (n > 2 && dst[n - 2] == '.' && dst[n - 1] == ' ') {
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


#if FF_WIN < 0x0600
static VOID WINAPI (*_GetSystemTimePreciseAsFileTime)(LPFILETIME);
void fftime_init(void)
{
	_GetSystemTimePreciseAsFileTime = (void*)ffdl_addr(GetModuleHandle(L"kernel32.dll"), "GetSystemTimePreciseAsFileTime");
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


int ffthd_join(ffthd th, uint timeout_ms, int *exit_code)
{
	int ret = WaitForSingleObject(th, timeout_ms);
	if (ret == WAIT_OBJECT_0) {
		if (exit_code != NULL)
			(void)GetExitCodeThread(th, (DWORD *)exit_code);
		(void)CloseHandle(th);
		ret = 0;
	} else if (ret == WAIT_TIMEOUT) {
		fferr_set(ETIMEDOUT);
		ret = ETIMEDOUT;
	}
	return ret;
}

int ffps_wait(ffps h, uint timeout_ms, int *exit_code)
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

/** Convert argv[] to command line string.
[ "filename", "arg space", "arg", NULL ] -> "filename \"arg space\" arg"
argv:
 Must not contain double quotes.
 Strings containing spaces are enclosed in double quotes.
Return a newly allocated string. */
static ffsyschar* argv_to_cmdln(const char **argv)
{
	ffsyschar *args, *s;
	ffbool has_space;
	size_t cap, i;

	cap = 0;
	for (i = 0;  argv[i] != NULL;  i++) {
		if (NULL != strchr(argv[i], '"')) {
			fferr_set(EINVAL);
			return NULL;
		}
		cap += ff_utow(NULL, 0, argv[i], -1, 0) + FFSLEN("\"\" ");
	}

	args = ffmem_alloc((cap + 1) * sizeof(ffsyschar));
	if (args == NULL)
		return NULL;

	s = args;
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
	if (i != 0)
		s--;
	*s = '\0';

	return args;
}

ffps ffps_exec2(const char *filename, ffps_execinfo *info)
{
	ffsyschar wfn_s[FF_MAXFN], *wfn, *args;
	ffps ps = FFPS_INV;
	size_t cap;

	cap = FFCNT(wfn_s);
	if (NULL == (wfn = ffs_utow(wfn_s, &cap, filename, -1)))
		return NULL;

	if (NULL == (args = argv_to_cmdln(info->argv)))
		goto end;

	ps = ffps_exec_cmdln_q2(wfn, args, info);

end:
	if (wfn != wfn_s)
		ffmem_free(wfn);
	ffmem_free(args);
	return ps;
}

static ffps ffps_exec_cmdln_q2(const ffsyschar *filename, ffsyschar *cmdln, ffps_execinfo *i)
{
	BOOL inherit_handles = 0;
	PROCESS_INFORMATION info;
	STARTUPINFO si = {};
	si.cb = sizeof(STARTUPINFO);

	if (i->in != FF_BADFD) {
		si.hStdInput = i->in;
		SetHandleInformation(i->in, HANDLE_FLAG_INHERIT, 1);
	}
	if (i->out != FF_BADFD) {
		si.hStdOutput = i->out;
		SetHandleInformation(i->out, HANDLE_FLAG_INHERIT, 1);
	}
	if (i->err != FF_BADFD) {
		si.hStdError = i->err;
		SetHandleInformation(i->err, HANDLE_FLAG_INHERIT, 1);
	}
	if (i->in != FF_BADFD || i->out != FF_BADFD || i->err != FF_BADFD) {
		si.dwFlags |= STARTF_USESTDHANDLES;
		inherit_handles = 1;
	}

	uint f = CREATE_UNICODE_ENVIRONMENT;
	BOOL b = CreateProcess(filename, cmdln, NULL, NULL, inherit_handles, f, /*env*/ NULL
		, /*startup dir*/ NULL, &si, &info);

	if (i->in != FF_BADFD) {
		SetHandleInformation(i->in, HANDLE_FLAG_INHERIT, 0);
	}
	if (i->out != FF_BADFD) {
		SetHandleInformation(i->out, HANDLE_FLAG_INHERIT, 0);
	}
	if (i->err != FF_BADFD) {
		SetHandleInformation(i->err, HANDLE_FLAG_INHERIT, 0);
	}

	if (!b)
		return FFPS_INV;
	CloseHandle(info.hThread);
	return info.hProcess;
}

ffps ffps_exec_cmdln_q(const ffsyschar *filename, ffsyschar *cmdln)
{
	ffps_execinfo info = {};
	info.in = info.out = info.err = FF_BADFD;
	return ffps_exec_cmdln_q2(filename, cmdln, &info);
}

ffps ffps_exec_cmdln(const char *filename, const char *cmdln)
{
	ffps ps = FFPS_INV;
	ffsyschar wfn_s[255], *wfn = NULL, wcmdln_s[255], *wcmdln = NULL;
	size_t cap;

	cap = FFCNT(wfn_s);
	if (NULL == (wfn = ffs_utow(wfn_s, &cap, filename, -1)))
		goto end;
	cap = FFCNT(wcmdln_s);
	if (NULL == (wcmdln = ffs_utow(wcmdln_s, &cap, cmdln, -1)))
		goto end;

	ps = ffps_exec_cmdln_q(wfn, wcmdln);

end:
	if (wfn != wfn_s)
		ffmem_free(wfn);
	if (wcmdln != wcmdln_s)
		ffmem_free(wcmdln);
	return ps;
}

/* Create a copy of the current process with @arg appended to process' command line. */
ffps ffps_createself_bg(const char *arg)
{
	fffd ps = FF_BADFD;
	size_t cap, arg_len = strlen(arg), psargs_len;
	ffsyschar *args, *p, *ps_args, fn[FF_MAXPATH];

	if (0 == GetModuleFileName(NULL, fn, FFCNT(fn)))
		return FF_BADFD;

	ps_args = GetCommandLine();
	psargs_len = ffq_len(ps_args);
	cap = psargs_len + FFSLEN(" ") + arg_len + 1;
	if (NULL == (args = ffq_alloc(cap)))
		return FF_BADFD;
	p = args;
	memcpy(p, ps_args, psargs_len * sizeof(ffsyschar));
	p += psargs_len;
	*p++ = ' ';
	p += ff_utow(p, args + cap - p, arg, arg_len, 0);
	*p++ = '\0';

	ps = ffps_exec_cmdln_q(fn, args);

	ffmem_free(args);
	return ps;
}


static byte _ffdl_noflags; //OS doesn't support flags to LoadLibraryEx()

int ffdl_init(const char *path)
{
	int rc = 0;
	ffsyschar *w = NULL, ws[FF_MAXFN];
	ffdl dl;
	if (NULL == (dl = ffdl_openq(L"kernel32.dll", 0)))
		return -1;
	if (NULL != ffdl_addr(dl, "AddDllDirectory"))
		goto end;

	rc = -1;
	size_t n = FFCNT(ws);
	if (NULL == (w = ffs_utow(ws, &n, path, -1)))
		goto end;
	if (!SetDllDirectory(w))
		goto end;

	_ffdl_noflags = 1;
	rc = 0;

end:
	if (w != ws)
		ffmem_free(w);
	ffdl_close(dl);
	return rc;
}

ffdl ffdl_openq(const ffsyschar *filename, uint flags)
{
	flags = (flags != 0 && !_ffdl_noflags) ? (flags) : LOAD_WITH_ALTERED_SEARCH_PATH;
	return LoadLibraryEx(filename, NULL, flags);
}

ffdl ffdl_open(const char *filename, int flags)
{
	fffd f;
	ffsyschar *w, ws[FF_MAXFN];
	size_t n = FFCNT(ws);
	if (NULL == (w = ffs_utow(ws, &n, filename, -1)))
		return FF_BADFD;

	f = ffdl_openq(w, flags);
	if (w != ws)
		ffmem_free(w);
	return f;
}


#if FF_WIN < 0x0600
typedef BOOL __stdcall (*GetQueuedCompletionStatusEx_t)(HANDLE, void*, long, void*, long, long);
static GetQueuedCompletionStatusEx_t _ffGetQueuedCompletionStatusEx;

void ffkqu_init(void)
{
	ffdl k;
	void *a;
	if (NULL == (k = ffdl_openq(L"kernel32.dll", 0)))
		return;
	if (NULL == (a = ffdl_addr(k, "GetQueuedCompletionStatusEx")))
		goto done;
	_ffGetQueuedCompletionStatusEx = a;
done:
	ffdl_close(k);
}

/* Use GetQueuedCompletionStatusEx() if available. */
int ffkqu_wait(fffd kq, ffkqu_entry *events, size_t eventsSize, const ffkqu_time *tmoutMs)
{
	if (_ffGetQueuedCompletionStatusEx != NULL) {
		ULONG num = 0;
		BOOL b = _ffGetQueuedCompletionStatusEx(kq, events, FF_TOINT(eventsSize), &num, *tmoutMs, 0);
		if (!b)
			return (fferr_last() == WAIT_TIMEOUT) ? 0 : -1;
		return (int)num;
	}

	BOOL b = GetQueuedCompletionStatus(kq, &events->dwNumberOfBytesTransferred
		, &events->lpCompletionKey, &events->lpOverlapped, *tmoutMs);
	if (!b) {
		if (fferr_last() == WAIT_TIMEOUT)
			return 0;
		return (events->lpOverlapped == NULL) ? -1 : 1;
	}
	return 1;
}
#endif


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


static int __stdcall _fftmr_thd(void *param);

fftmr fftmr_create(int flags)
{
	fftmr tmr = ffmem_tcalloc1(fftmr_s);
	if (tmr == NULL)
		return FF_BADTMR;

	if (NULL == (tmr->evt = CreateEvent(NULL, /*bManualReset*/ 0, /*bInitialState*/ 0, NULL)))
		goto err;

#if FF_WIN >= 0x0600
	tmr->htmr = CreateWaitableTimerEx(NULL, NULL, flags /*CREATE_WAITABLE_TIMER_MANUAL_RESET*/, TIMER_ALL_ACCESS);
#else
	tmr->htmr = CreateWaitableTimer(NULL, 0, NULL);
#endif
	if (tmr->htmr == NULL)
		goto err;

	if (FFTHD_INV == (tmr->thd = ffthd_create(&_fftmr_thd, tmr, 64 * 1024)))
		goto err;
	return tmr;

err:
	fftmr_close(tmr, FF_BADFD);
	return FF_BADTMR;
}

void __stdcall _fftmr_onfire(LPVOID arg, DWORD dwTimerLowValue, DWORD dwTimerHighValue)
{
	fftmr t = arg;
	ffkevpost p;
	p.fd = t->kq;
	ffkqu_post(&p, t->data);
}

enum TMR_CMD {
	TMR_EXIT = 0x80000001U,
	TMR_START,
	TMR_STOP,
};

/** Arm the timer and sleep in an alertable state to execute timer callback function. */
static int __stdcall _fftmr_thd(void *param)
{
	int r;
	fftmr_s *tmr = param;

	for (;;) {
		r = WaitForSingleObjectEx(tmr->evt, INFINITE, /*alertable*/ 1);
		if (r == WAIT_IO_COMPLETION) {

		} else if (r == WAIT_OBJECT_0) {

			// execute the command and set the result
			ffatom_fence_acq();
			uint cmd = FF_READONCE(tmr->ctl);
			FFDBG_PRINTLN(10, "cmd: %xu", cmd);

			switch (cmd) {

			case TMR_EXIT:
				r = 0;
				break;

			case TMR_START: {
				uint period = tmr->period;
				int64 due_ns100 = (int64)tmr->period * 1000 * -10;
				if (tmr->period < 0)
					period = 0;
				r = SetWaitableTimer(tmr->htmr, (LARGE_INTEGER*)&due_ns100, period, &_fftmr_onfire, tmr, /*fResume*/ 1);
				r = (r) ? 0 : (fferr_last() & ~0x80000000);
				break;
			}

			case TMR_STOP:
				r = CancelWaitableTimer(tmr->htmr);
				r = (r) ? 0 : (fferr_last() & ~0x80000000);
				break;

			default:
				r = 0;
			}

			FF_WRITEONCE(tmr->ctl, r);

			if (cmd == TMR_EXIT)
				break;
		} else {
			//Note: deadlock in _fftmr_cmd() can occur
			break;
		}
	}
	FFDBG_PRINTLN(5, "exit", 0);
	return 0;
}

/** Send command to a timer thread and get the result. */
static int _fftmr_cmd(fftmr tmr, uint cmd)
{
	FFDBG_PRINTLN(5, "cmd: %xu", cmd);
	FF_WRITEONCE(tmr->ctl, cmd);
	ffatom_fence_rel();
	if (!SetEvent(tmr->evt)) //wake up timer thread
		return -1;

	int r;
	if (0 != (r = ffatom_waitchange(&tmr->ctl, cmd)))
		fferr_set(r);
	FFDBG_PRINTLN(5, "cmd result: %xu", r);
	return r;
}

int fftmr_start(fftmr tmr, fffd qu, void *data, int periodMs)
{
	tmr->kq = qu;
	tmr->data = data;
	tmr->period = periodMs;
	return _fftmr_cmd(tmr, TMR_START);
}

int fftmr_stop(fftmr tmr, fffd kq)
{
	return _fftmr_cmd(tmr, TMR_STOP);
}

int fftmr_close(fftmr tmr, fffd kq)
{
	FF_SAFECLOSE(tmr->htmr, NULL, CloseHandle);

	if (tmr->thd != FFTHD_INV) {
		_fftmr_cmd(tmr, TMR_EXIT);
		ffthd_join(tmr->thd, -1, NULL);
	}

	FF_SAFECLOSE(tmr->evt, NULL, CloseHandle);
	ffmem_free(tmr);
	return 0;
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
	fffd p;
	byte b;

	pipename(name, FFCNT(name), pid);

	p = fffile_openq(name, O_WRONLY);
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

static uint64 ft_val(const FILETIME *ft)
{
	return ((uint64)ft->dwHighDateTime << 32) | ft->dwLowDateTime;
}
static void int_fftime(uint64 n, fftime *t)
{
	fftime_setsec(t, n / (1000000 * 10));
	fftime_setnsec(t, (n % (1000000 * 10)) * 100);
}

/** Don't link to psapi.dll statically.
Note: once loaded, it's never unloaded.
Note: not thread safe. */
static ffdl dl_psapi;
typedef BOOL WINAPI (*GetProcessMemoryInfo_t)(HANDLE, PROCESS_MEMORY_COUNTERS*, DWORD);
static GetProcessMemoryInfo_t _GetProcessMemoryInfo;

static ffdl dl_ntdll;
typedef NTSTATUS (WINAPI *NtQuerySystemInformation_t)(SYSTEM_INFORMATION_CLASS, PVOID, ULONG, PULONG);
NtQuerySystemInformation_t _NtQuerySystemInformation;

static ffbool psapi_load(void)
{
	if (NULL == (dl_psapi = ffdl_openq(L"psapi.dll", 0)))
		return 0;
	if (NULL == (_GetProcessMemoryInfo = (void*)ffdl_addr(dl_psapi, "GetProcessMemoryInfo")))
		return 0;
	return 1;
}

static ffbool ntdll_load(void)
{
	if (NULL == (dl_ntdll = ffdl_openq(L"ntdll.dll", 0)))
		return 0;
	if (NULL == (_NtQuerySystemInformation = (void*)ffdl_addr(dl_ntdll, "NtQuerySystemInformation")))
		return 0;
	return 1;
}

typedef struct _SYSTEM_THREAD_INFORMATION {
	LARGE_INTEGER Reserved1[3];
	ULONG Reserved2;
	PVOID StartAddress;
	CLIENT_ID ClientId;
	KPRIORITY Priority;
	LONG BasePriority;
	ULONG Reserved3; //ContextSwitchCount
	ULONG ThreadState;
	ULONG WaitReason;
} SYSTEM_THREAD_INFORMATION;

struct psinfo_s {
	SYSTEM_PROCESS_INFORMATION psinfo;
	SYSTEM_THREAD_INFORMATION threads[0];
};

/** Get information about all running processes and their threads. */
static void* ps_info_get(void)
{
	void *data;
	enum {
		N_PROC = 50,
		N_THD = 10,
	};
	uint cap = N_PROC * (sizeof(SYSTEM_PROCESS_INFORMATION) + N_THD * sizeof(SYSTEM_THREAD_INFORMATION));
	int r;
	ULONG n;

	if (_NtQuerySystemInformation == NULL)
		ntdll_load();
	if (_NtQuerySystemInformation == NULL)
		return NULL;

	for (uint i = 0;  i != 2;  i++) {
		if (NULL == (data = ffmem_alloc(cap))) {
			r = -1;
			break;
		}
		r = _NtQuerySystemInformation(SystemProcessInformation, data, cap, &n);
		if (r != STATUS_INFO_LENGTH_MISMATCH)
			break;

		ffmem_free0(data);
		cap = ff_align_ceil2(n, 4096);
	}

	if (r != 0 || n < sizeof(SYSTEM_PROCESS_INFORMATION)) {
		ffmem_safefree(data);
		return NULL;
	}

	return data;
}

/** Get the next psinfo_s object or return NULL if there's no more. */
static struct psinfo_s* ps_info_next(struct psinfo_s *pi)
{
	if (pi->psinfo.NextEntryOffset == 0)
		return NULL;
	return (void*)((byte*)pi + pi->psinfo.NextEntryOffset);
}


int ffps_perf(struct ffps_perf *p, uint flags)
{
	int rc = 0, r;
	HANDLE h = GetCurrentProcess();

	if (flags & FFPS_PERF_REALTIME) {
		ffclk_get(&p->realtime);
		ffclk_totime(&p->realtime);
	}

	if (flags & (FFPS_PERF_SEPTIME | FFPS_PERF_CPUTIME)) {
		FILETIME cre, ex, kern, usr;
		if (0 != (r = GetProcessTimes(h, &cre, &ex, &kern, &usr))) {
			uint64 nk, nu;
			nk = ft_val(&kern);
			nu = ft_val(&usr);
			int_fftime(nk, &p->systime);
			int_fftime(nu, &p->usertime);

			if (flags & FFPS_PERF_CPUTIME)
				int_fftime(nk + nu, &p->cputime);
		}
		rc |= !r;
	}

	if (flags & FFPS_PERF_RUSAGE) {
		IO_COUNTERS io;
		if (0 != (r = GetProcessIoCounters(h, &io))) {
			p->inblock = io.ReadOperationCount;
			p->outblock = io.WriteOperationCount;
		}
		rc |= !r;
	}

	if (flags & FFPS_PERF_RUSAGE) {
		PROCESS_MEMORY_COUNTERS mc;
		if (_GetProcessMemoryInfo == NULL)
			r = psapi_load();
		if (_GetProcessMemoryInfo != NULL
			&& 0 != (r = _GetProcessMemoryInfo(h, &mc, sizeof(mc)))) {

			p->pagefaults = mc.PageFaultCount;
			p->maxrss = mc.PeakWorkingSetSize / 1024;
		}
		rc |= !r;
	}

	if (flags & FFPS_PERF_RUSAGE) {
		void *data = ps_info_get();
		if (data != NULL) {
			uint cur_pid = GetCurrentProcessId();
			struct psinfo_s *pi;
			for (pi = data;  pi != NULL;  pi = ps_info_next(pi)) {
				if ((size_t)pi->psinfo.UniqueProcessId == cur_pid) {
					for (uint th = 0;  th != pi->psinfo.NumberOfThreads;  th++) {
						const SYSTEM_THREAD_INFORMATION *ti = &pi->threads[th];
						p->vctxsw += ti->Reserved3;
					}
					break;
				}
			}
			ffmem_free(data);
		}
	}

	p->ivctxsw = 0;
	return rc;
}

int ffthd_perf(struct ffps_perf *p, uint flags)
{
	int rc = 0, r;
	HANDLE h = GetCurrentThread();

	if (flags & (FFPS_PERF_SEPTIME | FFPS_PERF_CPUTIME)) {
		FILETIME cre, ex, kern, usr;
		if (0 != (r = GetThreadTimes(h, &cre, &ex, &kern, &usr))) {
			uint64 nk, nu;
			nk = ft_val(&kern);
			nu = ft_val(&usr);
			int_fftime(nk, &p->systime);
			int_fftime(nu, &p->usertime);

			if (flags & FFPS_PERF_CPUTIME)
				int_fftime(nk + nu, &p->cputime);
		}
		rc |= !r;
	}

	if (flags & FFPS_PERF_RUSAGE) {
		void *data = ps_info_get();
		if (data != NULL) {
			uint cur_pid = GetCurrentProcessId();
			uint cur_tid = GetCurrentThreadId();
			struct psinfo_s *pi;
			for (pi = data;  pi != NULL;  pi = ps_info_next(pi)) {
				if ((size_t)pi->psinfo.UniqueProcessId == cur_pid) {
					for (uint th = 0;  th != pi->psinfo.NumberOfThreads;  th++) {
						const SYSTEM_THREAD_INFORMATION *ti = &pi->threads[th];
						if ((size_t)ti->ClientId.UniqueThread == cur_tid) {
							p->vctxsw += ti->Reserved3;
							break;
						}
					}
					break;
				}
			}
			ffmem_free(data);
		}
	}

	p->maxrss = 0;
	p->inblock = 0;
	p->outblock = 0;
	p->ivctxsw = 0;
	return rc;
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

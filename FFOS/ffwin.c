/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/dir.h>
#include <FFOS/time.h>
#include <FFOS/socket.h>
#include <FFOS/thread.h>
#include <FFOS/timer.h>

#include <time.h>

HANDLE _ffheap;

fffd fffile_open(const ffsyschar *filename, int flags)
{
	enum { share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE };
	int mode = (flags & 0x0000000f);
	int access = (flags & 0x000000f0) << 24;
	int f = (flags & 0xffffff00) | FILE_ATTRIBUTE_NORMAL;

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

ffdir ffdir_open(ffsyschar *path, size_t cap, ffdirentry *ent)
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


int fferr_str(int code, ffsyschar *dst, size_t dst_cap)
{
	return FormatMessage(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK
		, 0, code, 0, (LPWSTR)dst, FF_TOINT(dst_cap), 0);
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
	return *(FILETIME *)&d;
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
	return 0;
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

/**
Copyright (c) 2013 Simon Zolin
*/

static FFINL int ffio_result(OVERLAPPED *ol) {
	DWORD t;
	BOOL b = GetOverlappedResult(NULL, ol, &t, 0);
	if (!b)
		return -1;
	return t;
}

enum FFKQU_F {
	FFKQU_ADD = 0
	, FFKQU_READ = 1
	, FFKQU_WRITE = 2
	, FFKQU_ERR = 4
};

typedef OVERLAPPED_ENTRY ffkqu_entry;

#define ffkqu_data(ev)  ((void*)(ev)->lpCompletionKey)


static FFINL fffd ffkqu_create() {
	fffd h = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	if (h == 0)
		h = FF_BADFD;
	return h;
}

#define ffkqu_attach(kq, fd, data, flags) \
	(0 == CreateIoCompletionPort((fffd)(fd), kq, (ULONG_PTR)(data), 0))

typedef uint ffkqu_time;

static FFINL const ffkqu_time* ffkqu_settm(ffkqu_time *t, uint ms) {
	*t = ms;
	return t;
}

#if FF_WIN >= 0x0600

static FFINL int ffkqu_wait(fffd kq, ffkqu_entry *events, size_t eventsSize, const ffkqu_time *tmoutMs) {
	ULONG num = 0;
	BOOL b = GetQueuedCompletionStatusEx(kq, events, FF_TOINT(eventsSize), &num, *tmoutMs, 0);
	return b ? (int)num : -1;
}

#else
FF_EXTN void ffkqu_init(void);
FF_EXTN int ffkqu_wait(fffd kq, ffkqu_entry *events, size_t eventsSize, const ffkqu_time *tmoutMs);
#endif

#define ffkqu_post_attach(kq)

static FFINL int ffkqu_post(fffd ioq, void* data, OVERLAPPED *ovl) {
	return (0 == PostQueuedCompletionStatus(ioq, 0 /*bytes transferred*/, (ULONG_PTR)data, ovl));
}

static FFINL int ffkqu_close(fffd qu) {
	return 0 == CloseHandle(qu);
}

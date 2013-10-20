/**
Copyright (c) 2013 Simon Zolin
*/

typedef OVERLAPPED_ENTRY ffkqu_entry;

#define ffkqu_data(ev)  ((void*)(ev)->lpCompletionKey)

static FFINL fffd ffkqu_create() {
	fffd h = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	if (h == 0)
		h = FF_BADFD;
	return h;
}

typedef uint ffkqu_time;

static FFINL const ffkqu_time* ffkqu_settm(ffkqu_time *t, uint ms) {
	*t = ms;
	return t;
}

static FFINL int ffkqu_wait(fffd kq, ffkqu_entry *events, size_t eventsSize, const ffkqu_time *tmoutMs) {
	ULONG num = 0;
	BOOL b = GetQueuedCompletionStatusEx(kq, events, FF_TOINT(eventsSize), &num, *tmoutMs, 0);
	return b ? num : -1;
}

static FFINL int ffkqu_post(fffd ioq, void* data, OVERLAPPED *ovl) {
	return (0 == PostQueuedCompletionStatus(ioq, 0 /*bytes transferred*/, (ULONG_PTR)data, ovl));
}

#define ffkqu_close(qu)  (0 == CloseHandle(qu))

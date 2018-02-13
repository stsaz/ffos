/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/time.h>
#include <FFOS/mem.h>
#include <FFOS/thread.h>


static FFINL int ffclk_get(fftime *result) {
	LARGE_INTEGER val;
	QueryPerformanceCounter(&val);
	result->sec = val.QuadPart;
	return 0;
}

FF_EXTN void ffclk_totime(fftime *t);

static FFINL void ffclk_diff(const fftime *start, fftime *diff)
{
	diff->sec -= start->sec;
	ffclk_totime(diff);
}


/**
A timer must be started within a separate thread, which sleeps in an alertable state to process timer events.
From this thread a message to kernel queue is sent.
Note that GetQueuedCompletionStatusEx(alertable=1) can't be used for a reliable timer,
 because it doesn't enter an alertable state if there's a finished I/O completion task.

timer thread:  SleepEx(alertable=1) etc. -> timer proc -> ffkqu_post()
main thread:   ffkqu_wait() -> timer handler
*/

typedef struct {
	fffd htmr;
	fffd kq;
	void *data;
	int period;
	uint ctl;
	ffthd thd;
	HANDLE evt;
} fftmr_s;

typedef fftmr_s* fftmr;

#define FF_BADTMR  NULL

FF_EXTN void __stdcall _fftmr_onfire(LPVOID arg, DWORD dwTimerLowValue, DWORD dwTimerHighValue);

FF_EXTN fftmr fftmr_create(int flags);

#define fftmr_read(tmr)

FF_EXTN int fftmr_stop(fftmr tmr, fffd kq);

FF_EXTN int fftmr_close(fftmr tmr, fffd kq);

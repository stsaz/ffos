/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/time.h>
#include <FFOS/mem.h>
#include <FFOS/thread.h>


static FFINL int ffclk_get(fftime *result) {
	LARGE_INTEGER val;
	int r = QueryPerformanceCounter(&val);
	if (r != 0) {
		result->s = val.LowPart;
		result->mcs = val.HighPart;
	}
	return r == 0;
}

FF_EXTN void ffclk_diff(const fftime *start, fftime *diff);


typedef struct {
	fffd htmr;
	fffd kq;
	void *data;
	int period;
	ffthd thd;
} fftmr_s, * fftmr;

#define FF_BADTMR  NULL

FF_EXTN void __stdcall _fftmr_onfire(LPVOID arg, DWORD dwTimerLowValue, DWORD dwTimerHighValue);

FF_EXTN fftmr fftmr_create(int flags);

#define fftmr_read(tmr)

FF_EXTN int fftmr_stop(fftmr tmr, fffd kq);

FF_EXTN int fftmr_close(fftmr tmr, fffd kq);

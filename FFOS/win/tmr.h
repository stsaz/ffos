/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/time.h>
#include <FFOS/mem.h>


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
} fftmr_s, * fftmr;

#define FF_BADTMR  NULL

FF_EXTN void __stdcall _fftmr_onfire(LPVOID arg, DWORD dwTimerLowValue, DWORD dwTimerHighValue);

FF_EXTN fftmr fftmr_create(int flags);

#define fftmr_read(tmr)

static FFINL int fftmr_stop(fftmr tmr, fffd qu) {
	return 0 == CancelWaitableTimer(tmr->htmr);
}

static FFINL int fftmr_close(fftmr tmr, fffd qu) {
	int r = (0 == CloseHandle(tmr->htmr));
	ffmem_free(tmr);
	return r;
}

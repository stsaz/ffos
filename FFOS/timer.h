/**
Timer.
Copyright (c) 2013 Simon Zolin
*/

#pragma once

#include <FFOS/queue.h>

#if defined FF_UNIX
#include <FFOS/unix/tmr.h>

#elif defined FF_WIN
#include <FFOS/win/tmr.h>
#endif

/** Start timer.
Windows: this function must be called inside the same thread that runs event loop. */
FF_EXTN int fftmr_start(fftmr tmr, fffd kq, void *udata, int period_ms);

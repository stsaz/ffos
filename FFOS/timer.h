/** ffos: timer
2020, Simon Zolin
*/

/*
fftimer_create
fftimer_close
fftimer_start
fftimer_stop
fftimer_consume
*/

#pragma once

#include <FFOS/queue.h>

#ifdef FF_WIN

/*
A timer must be started within a separate thread, which sleeps in an alertable state to process timer events.
From this thread a message to kernel queue is sent.
Note that GetQueuedCompletionStatusEx(alertable=1) can't be used for the reliable timer,
 because it doesn't enter an alertable state if there's a finished I/O completion task.

timer thread:  SleepEx(alertable=1) etc. -> timer proc -> ffkq_post()
main thread:   ffkq_wait() -> timer handler
*/

#include <FFOS/thread.h>
#include <FFOS/std.h>
#include <FFOS/error.h>
#include <ffbase/atomic.h>

typedef struct _fftimer {
	HANDLE htmr;
	ffkq kq;
	void *data;
	int period_ms;
	ffuint ctl;
	int result;
	ffthread thd;
	HANDLE evt;
	HANDLE evt_result;
} _fftimer;

typedef _fftimer* fftimer;
#define FFTIMER_NULL  NULL

/** Timer function: called by OS while we are sleeping in alertable state */
static void __stdcall _fftimer_onfire(LPVOID arg, DWORD dwTimerLowValue, DWORD dwTimerHighValue)
{
	(void)dwTimerLowValue; (void)dwTimerHighValue;
	_fftimer *tmr = (_fftimer*)arg;
	ffkq_postevent post = ffkq_post_attach(tmr->kq, tmr->data);
	int r = ffkq_post(post, tmr->data);
	(void)r;
	FF_ASSERT(r == 0);
}

enum {
	_FFTIMER_EXIT,
	_FFTIMER_START,
	_FFTIMER_STOP,
};

/**
. Wait until a command is received
* _FFTIMER_EXIT: exit thread
* _FFTIMER_START: Arm the timer and sleep in an alertable state to execute timer callback function
  Set result code in tmr->result field
* _FFTIMER_STOP: Stop the timer
  Set result code in tmr->result field
*/
static int __stdcall _fftimer_tread(void *param)
{
	_fftimer *tmr = (_fftimer*)param;

	for (;;) {
		int r = WaitForSingleObjectEx(tmr->evt, INFINITE, /*alertable*/ 1);

		if (r == WAIT_IO_COMPLETION) {

		} else if (r == WAIT_OBJECT_0) {

			// execute the command and set the result
			ffuint cmd = FFINT_READONCE(tmr->ctl);

			if (cmd == _FFTIMER_EXIT)
				break;

			r = 0;
			switch (cmd) {

			case _FFTIMER_START: {
				ffint64 due_ns100 = (ffint64)tmr->period_ms * 1000 * -10;
				ffuint period_ms = (tmr->period_ms >= 0) ? tmr->period_ms : 0;
				if (!SetWaitableTimer(tmr->htmr, (LARGE_INTEGER*)&due_ns100, period_ms, &_fftimer_onfire, tmr, /*fResume*/ 1))
					r = GetLastError();
				break;
			}

			case _FFTIMER_STOP:
				if (!CancelWaitableTimer(tmr->htmr))
					r = GetLastError();
				break;
			}

			FFINT_WRITEONCE(tmr->result, r);
			ffcpu_fence_release();
			if (!SetEvent(tmr->evt_result)) { // wake up main thread
				FF_ASSERT(0);
			}

		} else {
			FF_ASSERT(0);
			break;
		}
	}
	return 0;
}

/** Send command to a timer thread and get the result */
static int _fftimer_cmd(fftimer tmr, ffuint cmd)
{
	FFINT_WRITEONCE(tmr->ctl, cmd);
	ffcpu_fence_release();
	if (!SetEvent(tmr->evt)) // wake up timer thread
		return -1;

	if (cmd == _FFTIMER_EXIT)
		return 0;

	int r = WaitForSingleObject(tmr->evt_result, INFINITE);
	if (r != WAIT_OBJECT_0)
		return -1;

	r = FFINT_READONCE(tmr->result);
	SetLastError(r);
	return r;
}

static inline void fftimer_close(fftimer tmr, ffkq kq)
{
	(void)kq;
	CloseHandle(tmr->htmr);
	if (tmr->thd != FFTHREAD_NULL) {
		_fftimer_cmd(tmr, _FFTIMER_EXIT);
		ffthread_join(tmr->thd, -1, NULL);
	}
	CloseHandle(tmr->evt);
	CloseHandle(tmr->evt_result);
	ffmem_free(tmr);
}

static inline fftimer fftimer_create(int flags)
{
	(void)flags;
	_fftimer *tmr = ffmem_new(_fftimer);
	if (tmr == NULL)
		return FFTIMER_NULL;

	if (NULL == (tmr->htmr = CreateWaitableTimer(NULL, 0, NULL)))
		goto err;

	if (NULL == (tmr->evt = CreateEvent(NULL, 0, 0, NULL)))
		goto err;
	if (NULL == (tmr->evt_result = CreateEvent(NULL, 0, 0, NULL)))
		goto err;

	if (FFTHREAD_NULL == (tmr->thd = ffthread_create(&_fftimer_tread, tmr, 4 * 1024)))
		goto err;

	return tmr;

err:
	fftimer_close(tmr, NULL);
	return FFTIMER_NULL;
}

static inline int fftimer_start(fftimer tmr, ffkq kq, void *data, int period_ms)
{
	tmr->kq = kq;
	tmr->data = data;
	tmr->period_ms = period_ms;
	return _fftimer_cmd(tmr, _FFTIMER_START);
}

static inline int fftimer_stop(fftimer tmr, ffkq kq)
{
	(void)kq;
	return _fftimer_cmd(tmr, _FFTIMER_STOP);
}

static inline void fftimer_consume(fftimer tmr)
{
	(void)tmr;
}

#elif defined FF_LINUX

#include <sys/timerfd.h>
#include <errno.h>

typedef int fftimer;
#define FFTIMER_NULL  (-1)

static inline fftimer fftimer_create(int flags)
{
	return timerfd_create(CLOCK_MONOTONIC, flags);
}

static inline void fftimer_close(fftimer tmr, ffkq kq)
{
	(void)kq;
	close(tmr);
}

static inline int fftimer_start(fftimer tmr, ffkq kq, void *data, int period_ms)
{
	struct epoll_event e;
	e.events = EPOLLIN | EPOLLET;
	e.data.ptr = data;
	if (0 != epoll_ctl(kq, EPOLL_CTL_ADD, tmr, &e)
		&& errno != EEXIST)
		return -1;

	int neg = 0;
	if (period_ms < 0) {
		neg = 1;
		period_ms = -period_ms;
	}

	struct itimerspec its;
	its.it_value.tv_sec = period_ms / 1000;
	its.it_value.tv_nsec = (period_ms % 1000) * 1000000;

	if (neg)
		its.it_interval.tv_sec = its.it_interval.tv_nsec = 0;
	else
		its.it_interval = its.it_value;

	return timerfd_settime(tmr, 0, &its, NULL);
}

static inline int fftimer_stop(fftimer tmr, ffkq kq)
{
	(void)kq;
	const struct itimerspec ts = {};
	return timerfd_settime(tmr, 0, &ts, NULL);
}

static inline void fftimer_consume(fftimer tmr)
{
	ffint64 val;
	read(tmr, &val, sizeof(ffint64));
}

#else // BSD/macOS:

#include <sys/event.h>
#include <stdlib.h>

typedef ffssize fftimer;
#define FFTIMER_NULL  (-1)

static inline fftimer fftimer_create(int flags)
{
	(void)flags;
	void *tmr = ffmem_alloc(1);
	if (tmr == NULL)
		return FFTIMER_NULL;
	*(ffbyte*)tmr = 0;
	return (ffssize)tmr;
}

static inline void fftimer_close(fftimer tmr, ffkq kq)
{
	if ((ffssize)tmr == FFTIMER_NULL)
		return;

	if (*(ffbyte*)tmr == 1) {
		struct kevent kev;
		EV_SET(&kev, tmr, EVFILT_TIMER, EV_DELETE, 0, 0, (void*)-1);
		kevent(kq, &kev, 1, NULL, 0, NULL);
	}

	ffmem_free((void*)tmr);
}

static inline int fftimer_start(fftimer tmr, ffkq kq, void *data, int period_ms)
{
	int f = 0;
	if (period_ms < 0) {
		period_ms = -period_ms;
		f = EV_ONESHOT;
	}

	struct kevent kev;
	EV_SET(&kev, tmr, EVFILT_TIMER, EV_ADD | EV_ENABLE | f, 0, period_ms, data);
	if (0 != kevent(kq, &kev, 1, NULL, 0, NULL))
		return -1;
	*(ffbyte*)tmr = 1;
	return 0;
}

static inline int fftimer_stop(fftimer tmr, ffkq kq)
{
	struct kevent kev;
	EV_SET(&kev, tmr, EVFILT_TIMER, EV_DELETE, 0, 0, (void*)-1);
	if (0 != kevent(kq, &kev, 1, NULL, 0, NULL))
		return -1;
	*(ffbyte*)tmr = 0;
	return 0;
}

static inline void fftimer_consume(fftimer tmr)
{
	(void)tmr;
}

#endif


/** Create timer
Return FFTIMER_NULL on error */
static fftimer fftimer_create(int flags);

/** Destroy timer */
static void fftimer_close(fftimer tmr, ffkq kq);

/** Start timer
period_ms: interval
  <0: fire just once after -period_ms interval */
static int fftimer_start(fftimer tmr, ffkq kq, void *data, int period_ms);

/** Stop a timer */
static int fftimer_stop(fftimer tmr, ffkq kq);

/** Consume timer data: must be called on each timer signal */
static void fftimer_consume(fftimer tmr);

/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/test.h>
#include <FFOS/timer.h>
#include <FFOS/thread.h>
#include <FFOS/queue.h>
#include <FFOS/error.h>
#include <test/all.h>

#define x FFTEST_BOOL

int test_clock()
{
	fftime start
		, stop;

	FFTEST_FUNC;

	x(0 == ffclk_get(&start));
	ffthd_sleep(150);
	x(0 == ffclk_get(&stop));
	ffclk_diff(&start, &stop);
	x(stop.s == 0 && stop.mcs >= (150 - 25) * 1000);

	return 0;
}

static int tval;
static fftmr tmr1, tmr2;

static void tmr_func1() {
	tval -= 1;
	fftmr_read(tmr1);
}

static void tmr_func2() {
	tval += 2;
	fftmr_read(tmr2);
}

int test_timer()
{
	fffd kq;
	ffkqu_time tt;
	ffkqu_entry ent;
	int nevents;
	void (*func_ptr)();
	enum { tmrResol = 100 };

	FFTEST_FUNC;

	test_clock();

	kq = ffkqu_create();
	x(kq != FF_BADFD);
	ffkqu_settm(&tt, -1);

	tmr1 = fftmr_create(0);
	x(tmr1 != FF_BADTMR);
	tmr2 = fftmr_create(0);
	x(tmr2 != FF_BADTMR);

	x(0 == fftmr_start(tmr1, kq, &tmr_func1, -50));
	x(0 == fftmr_start(tmr2, kq, &tmr_func2, 100));

	for (;;) {
		nevents = ffkqu_wait(kq, &ent, 1, &tt);

		if (nevents > 0) {
			x(nevents == 1);
			func_ptr = ffkqu_data(&ent);
			func_ptr();
			if (tval == 2 * 2 - 1)
				break;
		}

		if (nevents == -1 && fferr_last() != EINTR) {
			x(0);
			break;
		}
	}

	(void)fftmr_stop(tmr1, kq);
	x(0 == fftmr_stop(tmr2, kq));
	x(0 == fftmr_close(tmr1, kq));
	x(0 == fftmr_close(tmr2, kq));

	x(0 == ffkqu_close(kq));

	return 0;
}

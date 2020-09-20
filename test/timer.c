/** ffos: timer.h tester
2020, Simon Zolin
*/

#include <FFOS/time.h>
#include <FFOS/timer.h>
#include <FFOS/test.h>

void test_time()
{
	fftime_zone tz;
	fftime_local(&tz);
	printf("zone: %d, have_dst: %u\n", tz.off, tz.have_dst);

	fftime t;
	fftime_now(&t);
	x(fftime_sec(&t) != 0);
}

void test_timer()
{
	ffkq kq = ffkq_create();
	x_sys(kq != FFKQ_NULL);

	fftimer tmr = fftimer_create(0);
	x_sys(tmr != FFTIMER_NULL);

	const int val = 200;


	// create periodic timer
	x_sys(0 == fftimer_start(tmr, kq, (void*)0x12345678, val));
	fflog("fftimer_start...");

	ffkq_event ev;
	ffkq_time tm;
	ffkq_time_set(&tm, val*2);
	xint_sys(1, ffkq_wait(kq, &ev, 1, tm));
	xieq(0x12345678, (ffsize)ffkq_event_data(&ev));
	fftimer_consume(tmr);
	fflog("timer signal");

	xint_sys(1, ffkq_wait(kq, &ev, 1, tm));
	xieq(0x12345678, (ffsize)ffkq_event_data(&ev));
	fftimer_consume(tmr);
	fflog("timer signal");

	x_sys(0 == fftimer_stop(tmr, kq));

	fflog("waiting after timer stop...");
	xint_sys(0, ffkq_wait(kq, &ev, 1, tm));


	// create one-shot timer
	x_sys(0 == fftimer_start(tmr, kq, (void*)0x12345678, -val));
	fflog("fftimer_start one-shot...");

	xint_sys(1, ffkq_wait(kq, &ev, 1, tm));
	xieq(0x12345678, (ffsize)ffkq_event_data(&ev));
	fftimer_consume(tmr);
	fflog("timer signal");

	fflog("waiting after one-shot timer...");
	ffkq_time_set(&tm, val*2);
	xint_sys(0, ffkq_wait(kq, &ev, 1, tm));


	fftimer_close(tmr, kq);
	ffkq_close(kq);
}

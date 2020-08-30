/** ffos: perf.h tester
2020, Simon Zolin
*/

#include <FFOS/perf.h>
#include <FFOS/timer-compat.h>
#include <FFOS/thread.h>
#include <FFOS/test.h>

void test_clock()
{
	fftime start
		, stop;

	FFTEST_FUNC;

	x(0 == ffclk_get(&start));
	ffthread_sleep(150);
	x(0 == ffclk_get(&stop));
	ffclk_diff(&start, &stop);
	x(fftime_sec(&stop) == 0 && fftime_usec(&stop) >= (150 - 25) * 1000);
}

void test_perf()
{
	test_clock();

	for (int i = 0;  i != 2;  i++) {

		struct ffps_perf p = {};
		if (i == 0)
			x_sys(0 == ffps_perf(&p, FFPS_PERF_REALTIME | FFPS_PERF_CPUTIME | FFPS_PERF_RUSAGE));
		else
#ifdef FF_APPLE
			x_sys(0 == ffthread_perf(&p, FFPS_PERF_REALTIME | FFPS_PERF_CPUTIME));
#else
			x_sys(0 == ffthread_perf(&p, FFPS_PERF_REALTIME | FFPS_PERF_CPUTIME | FFPS_PERF_RUSAGE));
#endif

		fflog("realtime:%u.%09u  cputime:%u.%09u  usertime:%u.%09u  systime:%u.%09u"
			"  maxrss:%u  inblock:%u  outblock:%u  vctxsw:%u  ivctxsw:%u"
			, (int)p.realtime.sec, (int)p.realtime.nsec
			, (int)p.cputime.sec, (int)p.cputime.nsec
			, (int)p.usertime.sec, (int)p.usertime.nsec
			, (int)p.systime.sec, (int)p.systime.nsec
			, p.maxrss, p.inblock, p.outblock, p.vctxsw, p.ivctxsw);
	}
}

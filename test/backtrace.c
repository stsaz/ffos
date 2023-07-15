/** ffos: backtrace.h tester
2020, Simon Zolin
*/

#include <FFOS/backtrace.h>
#include <FFOS/test.h>

void test_backtrace()
{
	ffthread_bt bt = {};
	ffuint n = ffthread_backtrace(&bt);
	x(n != 0);
	for (ffuint i = 0;  i != n;  i++) {

		const ffsyschar *name = ffthread_backtrace_modname(&bt, i);
		fflog("#%u: 0x%p %q [0x%p]"
			, i, ffthread_backtrace_frame(&bt, i)
			, name
			, ffthread_backtrace_modbase(&bt, i));
	}
}

/** ffos: thread.h tester
2020, Simon Zolin
*/

#include <FFOS/thread.h>
#include <FFOS/test.h>

static int FFTHREAD_PROCCALL thdfunc(void *param)
{
	xieq(0x12345, (ffsize)param);
	fflog("tid: %u", (int)ffthread_curid());
	ffthread_sleep(1000);
	return 1234;
}

void test_thread()
{
	ffthread th = ffthread_create(&thdfunc, (void*)0x12345, 64 * 1024);
	x_sys(th != FFTHREAD_NULL);

	fflog("waiting for thread...");
	int code;
	x(0 == ffthread_join(th, -1, &code));
	xieq(1234, code);

	ffthread_detach(th); // noop
}

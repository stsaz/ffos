/** ffos: semaphore.h tester
2020, Simon Zolin
*/

#include <FFOS/semaphore.h>
#include <FFOS/test.h>

void test_semaphore_unnamed()
{
	ffsem s;
	x_sys(FFSEM_NULL != (s = ffsem_open(NULL, 0, 0)));
	x(0 != ffsem_wait(s, 0));
	x(fferr_last() == ETIMEDOUT);

	x_sys(0 == ffsem_post(s));
	x_sys(0 == ffsem_post(s));
	// cnt=2

	x(0 == ffsem_wait(s, -1));
	x(0 == ffsem_wait(s, 0));
	// cnt=0
	x(0 != ffsem_wait(s, 0));

	ffsem_close(s);
	s = FFSEM_NULL;
	x(0 != ffsem_wait(s, 0));
}

void test_semaphore_named()
{
	const char *name = "/ffos-test.sem";
	ffsem s, s2;

	ffsem_unlink(name);

	x_sys(FFSEM_NULL == ffsem_open(name, 0, 0));
	x_sys(FFSEM_NULL != (s = ffsem_open(name, FFSEM_CREATENEW, 2)));
	x_sys(FFSEM_NULL != (s2 = ffsem_open(name, 0, 0)));
	x_sys(0 == ffsem_wait(s, 0));
	x_sys(0 == ffsem_wait(s2, 0));
	// cnt=0
	x(0 != ffsem_wait(s, 0) && fferr_last() == ETIMEDOUT);
	x(0 != ffsem_wait(s2, 0) && fferr_last() == ETIMEDOUT);

	x_sys(0 == ffsem_post(s));
	x_sys(0 == ffsem_post(s2));
	// cnt=2

#ifdef FF_APPLE
	x_sys(0 == ffsem_wait(s, 0));
#else
	x_sys(0 == ffsem_wait(s, 1000));
#endif
	x_sys(0 == ffsem_wait(s2, 0));
	// cnt=0
	x(0 != ffsem_wait(s, 100));
	x(0 != ffsem_wait(s2, 0));

	x(0 == ffsem_unlink(name));
	ffsem_close(s);
	ffsem_close(s2);
}

void test_semaphore()
{
	test_semaphore_unnamed();
	test_semaphore_named();
}

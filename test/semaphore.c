/** ffos: semaphore.h tester
2020, Simon Zolin
*/

#include <FFOS/semaphore.h>
#include <FFOS/test.h>

void test_semaphore_unnamed()
{
	ffsem s;
	x(FFSEM_INV != (s = ffsem_open(NULL, 0, 0)));
	x(0 != ffsem_wait(s, 0));
	x(fferr_last() == ETIMEDOUT);
	ffsem_close(s);
	s = FFSEM_INV;
	x(0 != ffsem_wait(s, 0));
}

void test_semaphore_named()
{
	const char *name = "/ffos-test.sem";
	ffsem s, s2;

	ffsem_unlink(name);

	x(FFSEM_INV == ffsem_open(name, 0, 0));
	x(FFSEM_INV != (s = ffsem_open(name, FFFILE_CREATENEW, 2)));
	x(FFSEM_INV != (s2 = ffsem_open(name, 0, 0)));
	x(0 == ffsem_wait(s, 0));
	x(0 == ffsem_wait(s2, 0));
	//cnt=0
	x(0 != ffsem_wait(s, 0) && fferr_last() == ETIMEDOUT);
	x(0 != ffsem_wait(s2, 0) && fferr_last() == ETIMEDOUT);

	x(0 == ffsem_post(s));
	x(0 == ffsem_post(s2));
	//cnt=2

	x(0 == ffsem_wait(s, 0));
	x(0 == ffsem_wait(s2, 0));
	//cnt=0
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

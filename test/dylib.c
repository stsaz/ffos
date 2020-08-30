/** ffos: dylib.h tester
2020, Simon Zolin
*/

#include <FFOS/dylib.h>
#include <FFOS/test.h>

void _test_dl(const char *fn, const char *func)
{
	ffdl h;
	void *p;

	h = ffdl_open(fn, 0);
	x_sys(h != NULL);
	p = ffdl_addr(h, func);
	x_sys(p != NULL);
	ffdl_close(h);
}

void test_dylib()
{
	x(FFDL_NULL == ffdl_open("not-found.so", 0));
	fflog("ffdl_open: %s", ffdl_errstr());

#ifdef FF_LINUX
	_test_dl("/lib64/libc.so.6", "open");

#elif defined FF_BSD
	_test_dl("/lib/libc.so.7", "open");

#elif defined FF_APPLE
	_test_dl("/usr/lib/libSystem.B.dylib", "open");

#else
	_test_dl("kernel32.dll", "CreateFileW");
#endif
}

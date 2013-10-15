/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/thread.h>
#include <FFOS/test.h>

#include <test/all.h>

#define x FFTEST_BOOL

static int FFTHDCALL thdfunc(void *param) {
	return 0;
}

int test_thd()
{
	ffthd th;

	FFTEST_FUNC;

	th = ffthd_create(&thdfunc, NULL, 64 * 1024);
	x(th != 0);
	x(0 == ffthd_detach(th));

	return 0;
}

int test_all()
{
	test_types();
	test_mem();
	test_time();
	test_file(TMP_PATH);
	test_dir(TMP_PATH);
	test_thd();
	test_skt();
	return 0;
}

int main(int argc, const char **argv)
{
	return test_all();
}

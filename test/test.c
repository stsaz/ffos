/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/thread.h>
#include <FFOS/test.h>
#include <FFOS/timer.h>
#include <FFOS/mem.h>

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

#define CALL(func)\
	do {\
		fftime start, stop;\
		ffclk_get(&start);\
		func;\
		ffclk_get(&stop);\
		ffclk_diff(&start, &stop);\
		printf("  %d.%06ds\n", stop.s, stop.mcs);\
	} while (0)

int test_all()
{
	ffos_init();

	CALL(test_types());
	CALL(test_mem());
	CALL(test_time());
	CALL(test_file(TMP_PATH));
	CALL(test_dir(TMP_PATH));
	CALL(test_thd());
	CALL(test_skt());
	CALL(test_timer());
	return 0;
}

int main(int argc, const char **argv)
{
	return test_all();
}

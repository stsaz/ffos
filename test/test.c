/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/thread.h>
#include <FFOS/test.h>
#include <FFOS/timer.h>
#include <FFOS/mem.h>
#include <FFOS/process.h>

#include <test/all.h>

#define x FFTEST_BOOL
#define CALL FFTEST_TIMECALL

uint _fftest_nrun;
uint _fftest_nfail;

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

int test_ps(const ffsyschar *fn)
{
	fffd h;
	int cod;
	const ffsyschar *args[6];
	const ffsyschar *env[4];

	FFTEST_FUNC;

	x(ffps_curid() == ffps_id(ffps_curhdl()));
	if (0) {
		ffps_exit(1);
		(void)ffps_kill(ffps_curhdl());
	}

	args[0] = fn;
#ifdef FF_UNIX
	args[1] = "good";
	args[2] = "day";
	args[3] = "";
	args[4] = NULL;
	env[0] = NULL;

#else
	args[1] = TEXT("/c");
	args[2] = TEXT("echo");
	args[3] = TEXT("good");
	args[4] = TEXT("day");
	args[5] = NULL;
	env[0] = NULL;
#endif

	h = ffps_exec(fn, args, env);
	x(h != FF_BADFD);
	printf("\ncreated process: %d\n", (int)ffps_id(h));
	ffthd_sleep(100);
	(void)ffps_kill(h);
	x(0 == ffps_wait(h, -1, &cod));
	printf("process exited with code %d\n", (int)cod);

	return 0;
}

int test_dl(const ffsyschar *fn, const char *func)
{
	ffdl h;
	ffdl_proc p;

	FFTEST_FUNC;

	h = ffdl_open(fn, 0);
	x(h != NULL);
	p = ffdl_addr(h, func);
	x(p != NULL);
	x(0 == ffdl_close(h));
	return 0;
}

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

#ifdef FF_UNIX
	CALL(test_ps(TEXT("/bin/echo")));
	CALL(test_dl(TEXT("/lib/i386-linux-gnu/libc.so.6"), "open"));
#else
	CALL(test_ps(TEXT("c:\\windows\\system32\\cmd.exe")));
	CALL(test_dl(TEXT("kernel32.dll"), "CreateFileW"));
#endif

	return 0;
}

int main(int argc, const char **argv)
{
	return test_all();
}

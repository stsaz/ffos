/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/thread.h>
#include <FFOS/test.h>
#include <FFOS/timer.h>
#include <FFOS/mem.h>
#include <FFOS/process.h>
#include <FFOS/random.h>
#include <FFOS/atomic.h>

#include <test/all.h>

#define x FFTEST_BOOL
#define CALL FFTEST_TIMECALL

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

int test_dl(const char *fn, const char *func)
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

int test_sconf()
{
	ffsysconf sc;
	ffsc_init(&sc);
	x(0 != ffsc_get(&sc, _SC_PAGESIZE));
	x(0 != ffsc_get(&sc, _SC_NPROCESSORS_ONLN));
	return 0;
}

static int test_rnd()
{
	int n, n2;
	fftime t;

	FFTEST_FUNC;

	fftime_now(&t);
	ffrnd_seed(t.s);
	n = ffrnd_get();
	n2 = ffrnd_get();
	x(n != n2);

	ffrnd_seed(t.s);
	n2 = ffrnd_get();
	x(n == n2);

	return 0;
}

static int test_atomic()
{
	ffatomic a;

	FFTEST_FUNC;

	ffatom_set(&a, 0x12345678);
	x(0x12345678 == ffatom_xchg(&a, 0x87654321));
	x(!ffatom_cmpxchg(&a, 0x11223344, 0xabcdef));
	x(ffatom_cmpxchg(&a, 0x87654321, 0xabcdef));
	x(0xabcdef == ffatom_get(&a));
	x(0xffabcdef == ffatom_addret(&a, 0xff000000));
	x(0xffabcdee == ffatom_decret(&a));
	x(0xffabcdef == ffatom_incret(&a));

#ifdef FF_64
	ffatom_set(&a, 0x12345678);
	x(0xffffffff12345678ULL == ffatom_addret(&a, 0xffffffff00000000ULL));

	ffatom_set(&a, 0x12345678);
	ffatom_add(&a, 0xffffffff00000000ULL);
	x(0xffffffff12345678ULL == ffatom_get(&a));

	ffatom_set(&a, 0xffffffffffffffffULL);
	ffatom_inc(&a);
	x(0 == ffatom_get(&a));

	ffatom_dec(&a);
	x(0xffffffffffffffffULL == ffatom_get(&a));

#else
	ffatom_set(&a, 0x12345678);
	ffatom_add(&a, 0x98000000);
	x(0xaa345678 == ffatom_get(&a));

	ffatom_inc(&a);
	x(0xaa345679 == ffatom_get(&a));

	ffatom_dec(&a);
	x(0xaa345678 == ffatom_get(&a));
#endif

	return 0;
}

static int test_lock()
{
	fflock lk;
	FFTEST_FUNC;

	fflk_init(&lk);
	x(0 != fflk_trylock(&lk));
	x(0 == fflk_trylock(&lk));
	fflk_unlock(&lk);
	return 0;
}

int test_all()
{
	ffos_init();

	CALL(test_types());
	CALL(test_atomic());
	CALL(test_lock());
	CALL(test_mem());
	CALL(test_time());
	CALL(test_file(TMP_PATH));
	CALL(test_dir(TMP_PATH));
	CALL(test_thd());
	CALL(test_skt());
	CALL(test_timer());
	CALL(test_kqu());

#ifdef FF_LINUX
	CALL(test_ps(TEXT("/bin/echo")));
	CALL(test_dl("/lib64/libc.so.6", "open"));

#elif defined FF_BSD
	CALL(test_ps(TEXT("/bin/echo")));
	CALL(test_dl("/lib/libc.so.7", "open"));

#else
	CALL(test_ps(TEXT("c:\\windows\\system32\\cmd.exe")));
	CALL(test_dl("kernel32.dll", "CreateFileW"));
#endif

	CALL(test_sconf());
	test_rnd();

	printf("%u tests were run, failed: %u.\n", fftestobj.nrun, fftestobj.nfail);
	return 0;
}

int main(int argc, const char **argv)
{
	return test_all();
}

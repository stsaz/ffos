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

static int test_ps(void)
{
	fffd h;
	int cod;
	const char *args[6];
	const char *env[4];

	FFTEST_FUNC;

	x(ffps_curid() == ffps_id(ffps_curhdl()));
	if (0) {
		ffps_exit(1);
		(void)ffps_kill(ffps_curhdl());
	}

#ifdef FF_UNIX
	args[0] = "/bin/echo";
	args[1] = "good";
	args[2] = "day";
	args[3] = "";
	args[4] = NULL;
	env[0] = NULL;

#else
	args[0] = "c:\\windows\\system32\\cmd.exe";
	args[1] = "/c";
	args[2] = "echo";
	args[3] = "very good";
	args[4] = "day";
	args[5] = NULL;
	env[0] = NULL;
#endif

	h = ffps_exec(args[0], args, env);
	x(h != FF_BADFD);
	printf("\ncreated process: %d\n", (int)ffps_id(h));
	x(0 == ffps_wait(h, -1, &cod));
	printf("process exited with code %d\n", (int)cod);


#ifdef FF_UNIX
	args[0] = "/bin/sh";
	args[1] = NULL;
#else
	args[0] = "c:\\windows\\system32\\cmd.exe";
	args[1] = NULL;
#endif

	x(FF_BADFD != (h = ffps_exec(args[0], args, env)));
	x(0 != ffps_wait(h, 0, &cod) && fferr_last() == ETIMEDOUT);
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

extern int test_fileaio(void);

struct test_s {
	const char *nm;
	int (*func)();
};

#define F(nm) { #nm, &test_ ## nm }
static const struct test_s _ffostests[] = {
	F(types), F(atomic), F(lock), F(mem), F(time), F(thd), F(sconf), F(rnd)
	, F(skt), F(timer), F(kqu), F(fileaio), F(ps), F(cpu),
#ifdef FF_WIN
	F(wreg),
#endif
};
#undef F

int main(int argc, const char **argv)
{
	size_t i, iarg;
	ffmem_init();

	fftestobj.flags |= FFTEST_FATALERR;

	if (argc == 1) {
		//run all tests
		for (i = 0;  i < FFCNT(_ffostests);  i++) {
			FFTEST_TIMECALL(_ffostests[i].func());
		}

	CALL(test_file(TMP_PATH));
	CALL(test_dir(TMP_PATH));

#ifdef FF_LINUX
	CALL(test_dl("/lib64/libc.so.6", "open"));

#elif defined FF_BSD
	CALL(test_dl("/lib/libc.so.7", "open"));

#else
	CALL(test_dl("kernel32.dll", "CreateFileW"));
#endif

	} else {
		//run the specified tests only

		for (iarg = 1;  iarg < argc;  iarg++) {
			for (i = 0;  i < FFCNT(_ffostests);  i++) {

				if (!strcmp(argv[iarg], _ffostests[i].nm)) {
					FFTEST_TIMECALL(_ffostests[i].func());
					break;
				}
			}
		}
	}

	printf("%u tests were run, failed: %u.\n", fftestobj.nrun, fftestobj.nfail);
	return 0;
}

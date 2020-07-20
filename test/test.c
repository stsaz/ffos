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
#include <FFOS/semaphore.h>
#include <FFOS/backtrace.h>
#include <FFOS/netconf.h>
#include <ffbase/stringz.h>
#include <test/test.h>

#include <test/all.h>

#undef x
#define x FFTEST_BOOL
#define CALL FFTEST_TIMECALL

static int FFTHDCALL thdfunc(void *param) {
	printf("tid: %u\n", (int)ffthd_curid());
	return 0;
}

int test_thd()
{
	ffthd th;

	FFTEST_FUNC;

	th = ffthd_create(&thdfunc, NULL, 64 * 1024);
	x(th != 0);
	x(0 == ffthd_join(th, -1, NULL));

	return 0;
}

static int test_ps(void)
{
	ffps h;
	int cod;
	const char *args[6];
	const char *env[4];

	FFTEST_FUNC;

	char fn[FF_MAXPATH];
	printf("ffps_filename(): %s\n", ffps_filename(fn, sizeof(fn), "/path/argv0"));

	x(ffps_curid() == ffps_id(ffps_curhdl()));
	if (0) {
		ffps_exit(1);
		(void)ffps_kill(ffps_curhdl());
	}

	uint i = 0;
#ifdef FF_UNIX
	args[i++] = "/bin/echo";

#else
	args[i++] = "c:\\windows\\system32\\cmd.exe";
	args[i++] = "/c";
	args[i++] = "echo";
#endif
	args[i++] = "very good";
	args[i++] = "day";
	args[i++] = NULL;
	env[0] = NULL;

	fffd pr, pw;
	x(0 == ffpipe_create(&pr, &pw));
	char buf[32];
	ffps_execinfo info;
	info.argv = args;
	info.env = env;
	info.in = FF_BADFD;
	info.out = pw;
	info.err = pw;
	h = ffps_exec2(args[0], &info);
	x(h != FFPS_INV);
	printf("\ncreated process: %d\n", (int)ffps_id(h));
	x(0 == ffps_wait(h, -1, &cod));
	printf("process exited with code %d\n", (int)cod);
#ifdef FF_UNIX
	x(14 == ffpipe_read(pr, buf, sizeof(buf)));
	buf[14] = '\0';
	x(!strcmp(buf, "very good day\n"));
#else
	x(17 == ffpipe_read(pr, buf, sizeof(buf)));
	buf[17] = '\0';
	x(!strcmp(buf, "\"very good\" day\r\n"));
#endif
	ffpipe_close(pr);
	ffpipe_close(pw);


#ifdef FF_UNIX
	args[0] = "/bin/sh";
	args[1] = NULL;
#else
	args[0] = "c:\\windows\\system32\\cmd.exe";
	args[1] = NULL;
#endif

	x(FFPS_INV != (h = ffps_exec(args[0], args, env)));
	x(0 != ffps_wait(h, 0, &cod) && fferr_last() == ETIMEDOUT);
	(void)ffps_kill(h);
	x(0 == ffps_wait(h, -1, &cod));
	printf("process exited with code %d\n", (int)cod);

	for (uint i = 0;  i != 2;  i++) {
		struct ffps_perf p = {};
		if (i == 0)
			x(0 == ffps_perf(&p, FFPS_PERF_CPUTIME | FFPS_PERF_RUSAGE));
		else
			x(0 == ffthd_perf(&p, FFPS_PERF_CPUTIME | FFPS_PERF_RUSAGE));
		printf("PID:%u TID:%u  cputime:%u.%09u  usertime:%u.%09u  systime:%u.%09u"
			"  maxrss:%u  inblock:%u  outblock:%u  vctxsw:%u  ivctxsw:%u\n"
			, (int)ffps_curid(), (int)ffthd_curid()
			, (int)fftime_sec(&p.cputime), (int)fftime_nsec(&p.cputime)
			, (int)fftime_sec(&p.usertime), (int)fftime_nsec(&p.usertime)
			, (int)fftime_sec(&p.systime), (int)fftime_nsec(&p.systime)
			, p.maxrss, p.inblock, p.outblock, p.vctxsw, p.ivctxsw);
	}

	return 0;
}

int test_env()
{
	char *p, buf[256];
	ffstr s;

#ifdef FF_UNIX
	char **ee = environ;
	char *e_s[] = { "KEY1=VAL1", "KEY=VAL", NULL };
	environ = e_s;

	p = ffenv_expand(NULL, NULL, 0, "asdf $KEY$KEY2 1234");
	ffstr_setz(&s, p);
	xseq(&s, "asdf VAL 1234");
	ffmem_free(p);

	p = ffenv_expand(NULL, buf, sizeof(buf), "asdf $KEY$KEY1 1234");
	x(p == buf);
	ffstr_setz(&s, p);
	xseq(&s, "asdf VALVAL1 1234");

	environ = ee;

#else
	p = ffenv_expand(NULL, buf, sizeof(buf), "asdf %WINDIR%%WINDIR1% 1234");
	x(p == buf);
	ffstr_setz(&s, p);
	xseq(&s, "asdf C:\\WINDOWS%WINDIR1% 1234");

	p = ffenv_expand(NULL, NULL, 0, "asdf %WINDIR%%WINDIR1% 1234");
	ffstr_setz(&s, p);
	xseq(&s, "asdf C:\\WINDOWS%WINDIR1% 1234");
	ffmem_free(p);
#endif

	x(0 != fflang_info(FFLANG_FLANG));
	return 0;
}

int test_netconf()
{
	ffnetconf nc = {};
	x(0 == ffnetconf_get(&nc, FFNETCONF_DNS_ADDR));
	x(nc.dns_addrs_num != 0);
	x(nc.dns_addrs[0] != NULL);
	x(nc.dns_addrs[0][0] != '\0');
	printf("dns_addrs[0]: '%s'\n", nc.dns_addrs[0]);
	ffnetconf_destroy(&nc);
	return 0;
}


static void test_semaphore_unnamed()
{
	ffsem s;
	x(FFSEM_INV != (s = ffsem_open(NULL, 0, 0)));
	x(0 != ffsem_wait(s, 0));
	x(fferr_last() == ETIMEDOUT);
	ffsem_close(s);
	s = FFSEM_INV;
	x(0 != ffsem_wait(s, 0));
}

static void test_semaphore_named()
{
	const char *name = "/ffos-test.sem";
	ffsem s, s2;

	ffsem_unlink(name);

	x(FFSEM_INV == ffsem_open(name, 0, 0));
	x(FFSEM_INV != (s = ffsem_open(name, FFO_CREATENEW, 2)));
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

static int test_semaphore(void)
{
	test_semaphore_unnamed();
	test_semaphore_named();
	return 0;
}

static int _test_dl(const char *fn, const char *func)
{
	ffdl h;
	ffdl_proc p;

	h = ffdl_open(fn, 0);
	x(h != NULL);
	p = ffdl_addr(h, func);
	x(p != NULL);
	x(0 == ffdl_close(h));
	return 0;
}

static int test_dl(void)
{
	FFTEST_FUNC;
#ifdef FF_LINUX
	_test_dl("/lib64/libc.so.6", "open");

#elif defined FF_BSD
	_test_dl("/lib/libc.so.7", "open");

#elif defined FF_APPLE
	_test_dl("/usr/lib/libSystem.B.dylib", "open");

#else
	_test_dl("kernel32.dll", "CreateFileW");
#endif
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
	ffrnd_seed(fftime_sec(&t));
	n = ffrnd_get();
	n2 = ffrnd_get();
	x(n != n2);

	ffrnd_seed(fftime_sec(&t));
	n2 = ffrnd_get();
	x(n == n2);

	return 0;
}

static int test_atomic()
{
	ffatomic a;

	FFTEST_FUNC;

	ffatom_set(&a, 0x12345678);
	x(0x12345678 == ffatom_swap(&a, 0x87654321));
	x(!ffatom_cmpset(&a, 0x11223344, 0xabcdef));
	x(ffatom_cmpset(&a, 0x87654321, 0xabcdef));
	x(0xabcdef == ffatom_get(&a));
	x(0xffabcdef == ffatom_addret(&a, 0xff000000));
	x(0xffabcdee == ffatom_decret(&a));
	x(0xffabcdef == ffatom_incret(&a));

	ffatom_set(&a, 0xabcdef);
	x(0xabcdef == ffatom_fetchadd(&a, 0xff000000));
	x(0xffabcdef == ffatom_get(&a));

	ffatom_set(&a, 0);
	ffatom_or(&a, 0x1000);
	x(ffatom_get(&a) == 0x1000);
	ffatom_and(&a, ~0x1000);
	x(ffatom_get(&a) == 0);

#ifdef FF_64
	ffatom_set(&a, 0x12345678);
	x(0x12345678 == ffatom_fetchadd(&a, 0xffffffff00000000ULL));
	x(0xffffffff12345678ULL == ffatom_get(&a));

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

int test_backtrace()
{
	FFTEST_FUNC;
	ffthd_bt bt = {};
	uint n = ffthd_backtrace(&bt);
	x(n != 0);
	for (uint i = 0;  i != n;  i++) {

#ifdef FF_UNIX
		const ffsyschar *name = ffthd_backtrace_modname(&bt, i);
#else
		char name[512];
		const ffsyschar *wname = ffthd_backtrace_modname(&bt, i);
		ff_wtou(name, sizeof(name), wname, -1, 0);
#endif

		printf("#%u: 0x%p %s [0x%p]\n"
			, i, ffthd_backtrace_frame(&bt, i)
			, name
			, ffthd_backtrace_modbase(&bt, i));
	}
	return 0;
}

extern int test_filemap();
extern int test_fileaio(void);
extern int test_dir();

struct test_s {
	const char *nm;
	int (*func)();
};

#define F(nm) { #nm, &test_ ## nm }
static const struct test_s _ffostests[] = {
	F(types), F(atomic), F(lock), F(mem), F(time), F(thd), F(sconf), F(rnd)
	, F(skt), F(timer), F(kqu), F(ps), F(semaphore), F(dl), F(cpu),
	F(file), F(filemap), F(fileaio),
	F(dir),
	F(netconf),
	F(env),
	F(backtrace),
#ifdef FF_WIN
	F(wreg),
#endif
};
#undef F

int main(int argc, const char **argv)
{
	size_t i, iarg;
	ffmem_init();

	fftestobj.flags |= FFTEST_STOPERR;

	if (argc == 1) {
		printf("Supported tests: all ");
		for (i = 0;  i != FFCNT(_ffostests);  i++) {
			printf("%s ", _ffostests[i].nm);
		}
		printf("\n");
		return 0;

	} else if (!strcmp(argv[1], "all")) {
		//run all tests
		for (i = 0;  i < FFCNT(_ffostests);  i++) {
			FFTEST_TIMECALL(_ffostests[i].func());
		}

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

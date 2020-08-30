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

void test_sconf()
{
	ffsysconf sc;
	ffsysconf_init(&sc);
	x(0 != ffsysconf_get(&sc, FFSYSCONF_PAGESIZE));
	x(0 != ffsysconf_get(&sc, FFSYSCONF_NPROCESSORS_ONLN));
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

/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/process.h>
#include <FFOS/random.h>
#include <FFOS/netconf.h>
#include <FFOS/sysconf.h>
#include <FFOS/test.h>
#include <FFOS/time.h>
#include <FFOS/ffos-extern.h>
#include <ffbase/stringz.h>

// void test_types()
// {
	// int v = 1;
	// x(FF_CMPSET(&v, 1, 2) && v == 2);
	// x(!FF_CMPSET(&v, 1, 0) && v == 2);
// }

void test_netconf()
{
	ffnetconf nc = {};
	x(0 == ffnetconf_get(&nc, FFNETCONF_DNS_ADDR));
	x(nc.dns_addrs_num != 0);
	x(nc.dns_addrs[0] != NULL);
	printf("dns_addrs[0]: '%s'\n", nc.dns_addrs[0]);
	ffnetconf_destroy(&nc);
}

void test_sysconf()
{
	ffsysconf sc;
	ffsysconf_init(&sc);
	x(0 != ffsysconf_get(&sc, FFSYSCONF_PAGESIZE));
	x(0 != ffsysconf_get(&sc, FFSYSCONF_NPROCESSORS_ONLN));
}

void test_rand()
{
	int n, n2;
	fftime t;

	FFTEST_FUNC;

	fftime_now(&t);
	ffrand_seed(fftime_sec(&t));
	n = ffrand_get();
	n2 = ffrand_get();
	x(n != n2);

	ffrand_seed(fftime_sec(&t));
	n2 = ffrand_get();
	x(n == n2);

}

#if 0
void test_atomic()
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

}
#endif

void test_atomic();
void test_backtrace();
void test_dir();
void test_dirscan();
void test_dylib();
void test_env();
void test_error();
void test_file();
void test_fileaio();
void test_filemap();
void test_kqu();
void test_kqueue();
void test_kcall();
void test_mem();
void test_netconf();
void test_netlink();
void test_path();
void test_perf();
void test_pipe();
void test_process();
void test_rand();
void test_sysconf();
void test_semaphore();
void test_socket();
void test_std();
void test_std_event();
void test_thread();
void test_time();
void test_timer();
void test_timerqueue();

void test_file_dosname();
void test_file_mount();
void test_sig_ctrlc();
void test_sig_abort();
void test_sig_segv();
void test_sig_stack();
void test_sig_fpe();
void test_unixsignal();
void test_winreg();

struct test_s {
	const char *name;
	void (*func)();
};

#define F(nm) { #nm, &test_ ## nm }
static const struct test_s atests[] = {
	F(backtrace),
	F(dir),
	F(dirscan),
	F(dylib),
	F(env),
	F(error),
	F(file),
	F(filemap),
	F(kqueue),
	F(kcall),
	F(mem),
	F(path),
	F(perf),
	F(pipe),
	F(process),
	F(rand),
	F(sysconf),
	F(semaphore),
	F(socket),
	F(std),
	F(std_event),
	F(thread),
	F(time),
	F(timer),
	F(timerqueue),
#ifdef FF_WIN
	F(winreg),
#else
	F(unixsignal),
#endif
	// F(atomic),
	// F(fileaio),
	// F(kqu),
};
static const struct test_s natests[] = {
	F(netconf), // network may be disabled
	F(file_dosname), // 8.3 support may be disabled globally
	F(file_mount), // need admin rights
	F(sig_ctrlc),
	F(sig_abort),
	F(sig_fpe),
	F(sig_segv),
	F(sig_stack),
	F(netlink),
};
#undef F

const char *ffostest_argv0;

int main(int argc, const char **argv)
{
	ffostest_argv0 = argv[0];
	const struct test_s *t;

	if (argc == 1) {
		ffstdout_fmt("Supported tests: all ");
		FFARRAY_FOREACH(atests, t) {
			ffstdout_fmt("%s ", t->name);
		}
		ffstdout_fmt("\nManual: ");
		FFARRAY_FOREACH(natests, t) {
			ffstdout_fmt("%s ", t->name);
		}
		ffstdout_fmt("\n");
		return 0;
	}

	if (ffsz_eq(argv[1], "all")) {
		//run all tests
		FFARRAY_FOREACH(atests, t) {
			ffstdout_fmt("%s\n", t->name);
			t->func();
			ffstdout_fmt("  OK\n");
		}

	} else {
		//run the specified tests only

		for (ffuint n = 1;  n < (ffuint)argc;  n++) {
			const struct test_s *sel = NULL;

			FFARRAY_FOREACH(atests, t) {
				if (ffsz_eq(argv[n], t->name)) {
					sel = t;
					goto call;
				}
			}

			FFARRAY_FOREACH(natests, t) {
				if (ffsz_eq(argv[n], t->name)) {
					sel = t;
					goto call;
				}
			}

			if (sel == NULL) {
				ffstdout_fmt("unknown test: %s\n", argv[n]);
				return 1;
			}

call:
			ffstdout_fmt("%s\n", sel->name);
			sel->func();
			ffstdout_fmt("  OK\n");
		}
	}
	return 0;
}

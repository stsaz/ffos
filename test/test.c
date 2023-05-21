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
#include <FFOS/../test/tests.h>
#define FFARRAY_FOREACH FF_FOREACH

ffuint _ffos_checks_success;
ffuint _ffos_keep_running;

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

	fftime_now(&t);
	ffrand_seed(fftime_sec(&t));
	n = ffrand_get();
	n2 = ffrand_get();
	x(n != n2);
}

struct test_s {
	const char *name;
	void (*func)();
};

// function declarations
#define X(NAME) extern void test_##NAME();
FFOS_TESTS_AUTO(X)
FFOS_TESTS_AUTO_OS(X)
FFOS_TESTS_MANUAL(X)
FFOS_TESTS_MANUAL_OS(X)
#undef X

// function tables
#define X(NAME) { #NAME, &test_##NAME },
static const struct test_s atests[] = {
	FFOS_TESTS_AUTO(X)
	FFOS_TESTS_AUTO_OS(X)
};
static const struct test_s natests[] = {
	FFOS_TESTS_MANUAL(X)
	FFOS_TESTS_MANUAL_OS(X)
};
#undef X

const char *ffostest_argv0;

int main(int argc, const char **argv)
{
	ffostest_argv0 = argv[0];
	const struct test_s *t;

	if (argc == 1) {
		fflog("Usage:\n  ffostest [-k] TEST...");
		ffstdout_fmt("Automatic tests: all ");
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

	ffuint k = 1;
	if (ffsz_eq(argv[k], "-k")) {
		k++;
		_ffos_keep_running = 1;
	}

	if (k == (ffuint)argc) {
		return 1;
	}

	if (ffsz_eq(argv[k], "all")) {
		//run all tests
		FFARRAY_FOREACH(atests, t) {
			ffstdout_fmt("%s\n", t->name);
			t->func();
			ffstdout_fmt("  OK\n");
		}

	} else {
		//run the specified tests only

		for (ffuint n = k;  n < (ffuint)argc;  n++) {
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

	fflog("Successful checks:\t%u", _ffos_checks_success);
	return 0;
}

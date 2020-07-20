/**
Unit testing.
Copyright (c) 2013 Simon Zolin
*/

#pragma once

#include <FFOS/types.h>
#include <FFOS/error.h>
#include <FFOS/timer.h>
#include <stdio.h>
#include <stdlib.h>


enum FFTEST_F {
	FFTEST_TRACE = 1
	, FFTEST_FATALERR = 2
	,
	FFTEST_STOPERR = 4, //stop execution on first error
};

typedef struct fftest {
	int flags; //enum FFTEST_F
	uint nrun;
	uint nfail;
} fftest;

FF_EXTN fftest fftestobj;

#define FFTEST_FUNC\
	printf("at %s:%u in %s()\n", __FILE__, __LINE__, FF_FUNC)

#define FFTEST_BOOL(expr)\
	fftest_chk(expr, __FILE__, __LINE__, FF_FUNC, #expr)

#define FFTEST_EQ(p1, p2)\
	fftest_chk(p1 == p2, __FILE__, __LINE__, FF_FUNC, #p1 " == " #p2)

FF_EXTN int fftest_chk(int res, const char *file, uint line, const char *func, const char *sexp);

static FFINL void fftest_tm(fftime *start) {
	fftime stop = {};
	ffclk_get(&stop);
	ffclk_diff(start, &stop);
	printf("  %u.%06us\n", (int)fftime_sec(&stop), (int)fftime_usec(&stop));
}

#define FFTEST_TIMECALL(func) \
do { \
	fftime start = {}; \
	ffclk_get(&start); \
	func; \
	fftest_tm(&start); \
} while (0)

static inline void test_check_sys(int ok, const char *expr, const char *file, ffuint line, const char *func)
{
	if (ok) {
		return;
	}

	fprintf(stderr, "FAIL: %s:%u: %s: %s (%d %s)\n"
		, file, line, func, expr
		, (int)fferr_last(), fferr_strp(fferr_last())
		);
	abort();
}

static inline void test_check_int_int_sys(int ok, ffint64 i1, ffint64 i2, const char *file, ffuint line, const char *func)
{
	if (ok) {
		return;
	}

#if defined _WIN32 || defined _WIN64 || defined __CYGWIN__
	fprintf(stderr, "FAIL: %s:%u: %s: %d != %d (%d %s)\n"
		, file, line, func
		, (int)i1, (int)i2
		, (int)fferr_last(), fferr_strp(fferr_last())
		);
#else
	fprintf(stderr, "FAIL: %s:%u: %s: %lld != %lld (%d %s)\n"
		, file, line, func
		, i1, i2
		, fferr_last(), fferr_strp(fferr_last())
		);
#endif
	abort();
}

#define xieq_sys(i1, i2) \
({ \
	ffint64 __i1 = (i1); \
	ffint64 __i2 = (i2); \
	test_check_int_int_sys(__i1 == __i2, __i1, __i2, __FILE__, __LINE__, __func__); \
})

#define x_sys(expr) \
({ \
	test_check_sys(expr, #expr, __FILE__, __LINE__, __func__); \
})

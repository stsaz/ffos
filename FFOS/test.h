/**
Unit testing.
Copyright (c) 2013 Simon Zolin
*/

#pragma once

#include <FFOS/types.h>
#include <FFOS/error.h>
#include <FFOS/timer.h>
#include <stdio.h>


enum FFTEST_F {
	FFTEST_TRACE = 1
	, FFTEST_FATALERR = 2
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
	fftime stop = {0};
	ffclk_get(&stop);
	ffclk_diff(start, &stop);
	printf("  %u.%06us\n", (int)fftime_sec(&stop), (int)fftime_usec(&stop));
}

#define FFTEST_TIMECALL(func) \
do { \
	fftime start = {0}; \
	ffclk_get(&start); \
	func; \
	fftest_tm(&start); \
} while (0)

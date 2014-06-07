/**
Unit testing.
Copyright (c) 2013 Simon Zolin
*/

#pragma once

#include <FFOS/types.h>
#include <FFOS/error.h>
#include <FFOS/timer.h>
#include <stdio.h>

FF_EXTN uint _fftest_nrun;
FF_EXTN uint _fftest_nfail;

#define FFTEST_FUNC\
	printf("at %s:%u in %s()\n", __FILE__, __LINE__, FF_FUNC)

#define FFTEST_BOOL(expr)\
	fftest(expr, __FILE__, __LINE__, FF_FUNC, #expr)

#define FFTEST_EQ(p1, p2)\
	fftest(p1 == p2, __FILE__, __LINE__, FF_FUNC, #p1 " == " #p2)

static FFINL int fftest(int res, const char *file, uint line, const char *func, const char *sexp)
{
	const char *status = "OK";
	if (res == 0) {
		status = "FAIL";
		printf("%s\tat %s:%u in %s():\t%s\t(%d)\n"
			, status, file, (int)line, func, sexp, (int)fferr_last());
		_fftest_nfail++;
	}
	_fftest_nrun++;
	return res;
}

static FFINL void fftest_tm(fftime *start) {
	fftime stop;
	ffclk_get(&stop);
	ffclk_diff(start, &stop);
	printf("  %u.%06us\n", stop.s, stop.mcs);
}

#define FFTEST_TIMECALL(func) \
do { \
	fftime start; \
	ffclk_get(&start); \
	func; \
	fftest_tm(&start); \
} while (0)

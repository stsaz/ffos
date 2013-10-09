/**
Unit testing.
Copyright (c) 2013 Simon Zolin
*/

#pragma once

#include <FFOS/types.h>
#include <stdio.h>

#define FFTEST_FUNC\
	printf("at %s:%u in %s()\n", __FILE__, __LINE__, __FUNCTION__)

#define FFTEST_BOOL(expr)\
	fftest(expr, __FILE__, __LINE__, __FUNCTION__, #expr)

#define FFTEST_EQ(p1, p2)\
	fftest(p1 == p2, __FILE__, __LINE__, __FUNCTION__, #p1 " == " #p2)

static int fftest(int res, const char *file, uint line, const char *func, const char *sexp)
{
	const char *status = "OK";
	if (res == 0) {
		status = "FAIL";
		printf("%s\tat %s:%u in %s():\t%s\n"
			, status, file, (int)line, func, sexp);
	}
	return res;
}

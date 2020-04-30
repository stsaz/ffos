/** Get backtrace info.
Copyright (c) 2020 Simon Zolin
*/

#pragma once

#ifdef FF_UNIX
#include <FFOS/unix/bt.h>
#else
#include <FFOS/win/bt.h>
#endif


/** Get backtrace info */
static int ffthd_backtrace(ffthd_bt *bt);

#define ffthd_backtrace_frame(bt, i)  ((bt)->frames[i])

/** Get module base address.
Return NULL on error. */
static void* ffthd_backtrace_modbase(ffthd_bt *bt, uint i);

/** Get module path.
Return NULL on error. */
static const ffsyschar* ffthd_backtrace_modname(ffthd_bt *bt, uint i);

#ifdef _DEBUG
#include <stdio.h>
static inline void _ffthd_backtrace_print(uint limit)
{
	ffthd_bt bt = {};
	uint n = ffthd_backtrace(&bt);
	if (limit != 0 && limit < n)
		n = limit;
	for (uint i = 0;  i != n;  i++) {
#ifdef FF_WIN
		printf(" #%u %p +%x %ls %p\n"
#else
		printf(" #%u %p +%x %s %p\n"
#endif
			, i
			, ffthd_backtrace_frame(&bt, i)
			, (int)((char*)ffthd_backtrace_frame(&bt, i) - (char*)ffthd_backtrace_modbase(&bt, i))
			, ffthd_backtrace_modname(&bt, i)
			, ffthd_backtrace_modbase(&bt, i)
			);
	}
}
#endif

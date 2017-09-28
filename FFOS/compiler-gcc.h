/** GCC functions.
Copyright (c) 2017 Simon Zolin
*/

#pragma once

#include <FFOS/types.h>


typedef long long int64;
typedef unsigned long long uint64;

#define FF_FUNC __func__

#define FF_EXP  __attribute__((visibility("default")))


/** Swap bytes in integer number.
e.g. "0x11223344" <-> "0x44332211" */
#define ffint_bswap16  __builtin_bswap16
#define ffint_bswap32  __builtin_bswap32
#define ffint_bswap64  __builtin_bswap64


#define ffbit_ffs32(i)  __builtin_ffs(i)

#ifdef FF_64
#define ffbit_ffs64(i)  __builtin_ffsll(i)
#endif

static FFINL uint ffbit_find32(uint n)
{
	return (n == 0) ? 0 : __builtin_clz(n) + 1;
}

#ifdef FF_64
static FFINL uint ffbit_find64(uint64 n)
{
	return (n == 0) ? 0 : __builtin_clzll(n) + 1;
}
#endif


/** Prevent compiler from reordering operations. */
#define ff_compiler_fence()  __asm volatile("" : : : "memory")

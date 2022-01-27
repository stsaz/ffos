/** CPU-specific code; CPUID.
Copyright (c) 2017 Simon Zolin
*/

#pragma once

#include <FFOS/types.h>
#include <ffbase/atomic.h>


typedef struct { uint val; } ffatomic32;

#if defined FF_X86
#include <FFOS/cpu-i386.h>
#include <ffbase/cpuid.h>

#elif defined FF_AMD64
#include <FFOS/cpu-i386.h>
#include <FFOS/cpu-amd64.h>
#include <ffbase/cpuid.h>

#elif defined FF_ARM
#include <FFOS/cpu-arm.h>

#endif

enum {
	/** Max positive value of a signed integer */
	FFINT_SMAX32 = 0x7fffffff,
	FFINT_SMAX64 = 0x7fffffffffffffffLL,
#ifdef FF_64
	FFINT_SMAX = FFINT_SMAX64,
#else
	FFINT_SMAX = FFINT_SMAX32,
#endif

	FFCPU_CACHELINE = 64,
};

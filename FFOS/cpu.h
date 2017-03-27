/** CPUID.
Copyright (c) 2017 Simon Zolin
*/

#pragma once

#include <FFOS/types.h>

#if defined FF_MSVC || defined FF_MINGW
#include <intrin.h>
#else
#include <cpuid.h>
#endif


enum _FFCPUID_FEAT {
	//ecx
	FFCPUID_SSE3 = 0,
	FFCPUID_SSSE3 = 9,
	FFCPUID_SSE41 = 19,
	FFCPUID_SSE42 = 20,
	//edx
	FFCPUID_MMX = 32 + 23,
	FFCPUID_SSE = 32 + 25,
	FFCPUID_SSE2 = 32 + 26,
};

typedef struct ffcpuid {
	uint maxid;
	uint maxid_ex;

	char vendor[12 + 4];
	uint features[2]; //enum _FFCPUID_FEAT
	char brand[3 * 16 + 4];
} ffcpuid;

#if defined FF_MSVC || defined FF_MINGW
#define _ffcpuid(level, eax, ebx, ecx, edx) \
do { \
	int info[4]; \
	__cpuid(info, level); \
	eax = info[0]; \
	ebx = info[1]; \
	ecx = info[2]; \
	edx = info[3]; \
} while (0)

#else
#define _ffcpuid  __cpuid
#endif

enum FFCPUID_FLAGS {
	FFCPUID_VENDOR = 1, //0
	FFCPUID_FEATURES = 2, //1
	FFCPUID_BRAND = 4, //0x80000002..0x80000004
	_FFCPUID_EXT = FFCPUID_BRAND //0x8xxxxxxx
};

/** Get CPU information.
@flags: enum FFCPUID_FLAGS.
Return 0 on success. */
static FFINL int ff_cpuid(ffcpuid *c, uint flags)
{
	int r = flags;
	uint eax, ebx, ecx, edx;

	_ffcpuid(0, eax, ebx, ecx, edx);
	c->maxid = eax;

	if (flags & FFCPUID_VENDOR) {
		uint *p = (uint*)c->vendor;
		*p++ = ebx;
		*p++ = edx;
		*p++ = ecx;
		r &= ~FFCPUID_VENDOR;
	}

	if ((flags & FFCPUID_FEATURES) && c->maxid >= 1) {
		_ffcpuid(1, eax, ebx, ecx, edx);
		c->features[0] = ecx;
		c->features[1] = edx;
		r &= ~FFCPUID_FEATURES;
	}

	if (flags & _FFCPUID_EXT) {
		_ffcpuid(0x80000000, eax, ebx, ecx, edx);
		c->maxid_ex = eax;
	}

	if ((flags & FFCPUID_BRAND) && c->maxid_ex >= 0x80000004) {
		uint *p = (uint*)c->brand;

		_ffcpuid(0x80000002, eax, ebx, ecx, edx);
		*p++ = eax;
		*p++ = ebx;
		*p++ = ecx;
		*p++ = edx;

		_ffcpuid(0x80000003, eax, ebx, ecx, edx);
		*p++ = eax;
		*p++ = ebx;
		*p++ = ecx;
		*p++ = edx;

		_ffcpuid(0x80000004, eax, ebx, ecx, edx);
		*p++ = eax;
		*p++ = ebx;
		*p++ = ecx;
		*p++ = edx;

		r &= ~FFCPUID_BRAND;
	}

	return r;
}

/**
@val: enum _FFCPUID_FEAT.
Return 1 if feature is supported.
Note: the same as ffbit_testarr(). */
static FFINL int ff_cpuid_feat(ffcpuid *c, uint val)
{
	return !!(c->features[val / 32] & (1 << (val % 32)));
}

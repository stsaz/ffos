/**
Copyright (c) 2017 Simon Zolin
*/

#include <FFOS/cpu.h>
#include <FFOS/atomic.h>
#include <FFOS/test.h>


#define x FFTEST_BOOL

int test_cpu(void)
{
	ffcpuid c = {0};
	x(0 == ff_cpuid(&c, FFCPUID_VENDOR | FFCPUID_FEATURES | FFCPUID_BRAND));
	printf("CPU:%s\nBrand:%s\n"
		"SSE:%u\nSSE2:%u\nSSE3:%u\nSSSE3:%u\nSSE4.1:%u\nSSE4.2:%u\n"
		, c.vendor, c.brand
		, ff_cpuid_feat(&c, FFCPUID_SSE)
		, ff_cpuid_feat(&c, FFCPUID_SSE2)
		, ff_cpuid_feat(&c, FFCPUID_SSE3)
		, ff_cpuid_feat(&c, FFCPUID_SSSE3)
		, ff_cpuid_feat(&c, FFCPUID_SSE41)
		, ff_cpuid_feat(&c, FFCPUID_SSE42));

#ifdef FF_WIN
	x(0 != ffcpu_rdtsc());
#else
	printf("TSC:%llx\n"
		, ffcpu_rdtsc());
#endif

	return 0;
}

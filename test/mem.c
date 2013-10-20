/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/mem.h>
#include <FFOS/string.h>
#include <FFOS/error.h>
#include <FFOS/test.h>

#include <string.h>

#define x FFTEST_BOOL

static int test_str()
{
	ffsyschar ss[1024];

	FFTEST_FUNC;

	fferr_set(EINVAL);
	x(0 != fferr_str(fferr_last(), ss, FFCNT(ss)));

	x(0 == ffq_icmp(TEXT("asdf"), FFSTRQ("ASDF")));
	x(FFSLEN("asdf") == ffq_len(TEXT("asdf")));

#ifdef FF_WIN
	{
		char s[1024];
		size_t n;
		n = ff_utow(ss, FFCNT(ss), FFSTR("asdf"), 0);
		x(n != 0);
		n = ff_wtou(s, FFCNT(s), ss, n, 0);
		x(n != 0);
	}
#endif

	return 0;
}

int test_mem()
{
	char ss10[10] = { 0,1,2,3,4,5,6,7,8,9 };
	char ss0[10] = { 0 };
	char *d;

	FFTEST_FUNC;

	d = ffmem_alloc(10);
	x(d != NULL);
	memcpy(d, ss10, 10);
	d = ffmem_realloc(d, 2000);
	x(d != NULL);
	x(!memcmp(d, ss10, 10));
	ffmem_free(d);

	d = ffmem_calloc(1, 10);
	x(d != NULL);
	x(!memcmp(d, ss0, 10));
	ffmem_free(d);

	d = ffmem_realloc(NULL, 10);
	x(d != NULL);
	ffmem_free(d);

	x(NULL == ffmem_alloc((size_t)-1));
	x(NULL == ffmem_calloc((size_t)-1, 1));
	x(NULL == ffmem_realloc(NULL, (size_t)-1));

	test_str();

	return 0;
}

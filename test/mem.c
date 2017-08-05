/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/mem.h>
#include <FFOS/string.h>
#include <FFOS/error.h>
#include <FFOS/test.h>

#define x FFTEST_BOOL

static int test_str()
{
	char su[1024];

	FFTEST_FUNC;

	fferr_set(EINVAL);
	x(0 == fferr_str(fferr_last(), su, FFCNT(su)));

	x(0 == ffq_icmpnz(TEXT("asdf"), FFSTRQ("ASDF")));
	x(FFSLEN("asdf") == ffq_len(TEXT("asdf")));

#ifdef FF_WIN
	{
		char s[1024];
		size_t n;
		WCHAR *pw;
		ffsyschar ss[1024];
		n = ff_utow(ss, FFCNT(ss), FFSTR("asdf"), 0);
		x(n != 0);

		n = 5;
		x(ss == (pw = ffs_utow(ss, &n, FFSTR("asdf"))));
		pw[n] = L'\0';
		x(n == FFSLEN("asdf") && !ffq_cmpz(pw, L"asdf"));

		n = 3;
		pw = ffs_utow(ss, &n, "asdf", -1);
		x(pw != NULL && pw != ss);
		x(n == FFSLEN("asdf")+1 && !ffq_cmpz(pw, L"asdf"));
		ffmem_free(pw);
		n = ff_wtou(s, FFCNT(s), L"asdf", 4, 0);
		x(n == 4 && !memcmp(s, "asdf", 4));
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

#ifndef FF_BSD
	x(NULL == ffmem_alloc((size_t)-1));
	x(NULL == ffmem_calloc((size_t)-1, 1));
	x(NULL == ffmem_realloc(NULL, (size_t)-1));
#endif

//align
	x(NULL != (d = ffmem_align(4, 16)));
	x(((size_t)d % 16) == 0);
	ffmem_alignfree(d);

	x(NULL != (d = ffmem_align(999, 512)));
	x(((size_t)d % 512) == 0);
	ffmem_alignfree(d);

	test_str();

	return 0;
}

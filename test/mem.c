/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/string.h>
#include <FFOS/error.h>
#include <FFOS/test.h>


static int test_str()
{
	char su[1024];

	FFTEST_FUNC;

	fferr_set(EINVAL);
	x(0 == fferr_str(fferr_last(), su, FF_COUNT(su)));

#ifdef FF_WIN
	x(0 == ffq_icmpnz(L"asdf", FFSTRQ("ASDF")));
	x(FFS_LEN("asdf") == ffq_len(L"asdf"));
	{
		size_t n;
		WCHAR *pw;
		ffsyschar ss[1024];
		n = ff_utow(ss, FF_COUNT(ss), FFSTR("asdf"), 0);
		x(n != 0);

		n = 5;
		x(ss == (pw = ffs_utow(ss, &n, FFSTR("asdf"))));
		pw[n] = L'\0';
		x(n == FFS_LEN("asdf") && !ffq_cmpz(pw, L"asdf"));

		n = 3;
		pw = ffs_utow(ss, &n, "asdf", -1);
		x(pw != NULL && pw != ss);
		x(n == FFS_LEN("asdf")+1 && !ffq_cmpz(pw, L"asdf"));
		ffmem_free(pw);
	}

	ffsize n = 2;
	wchar_t *ws = ffs_alloc_utow_addcap("asdf", 3, &n);
	xieq(3, n);
	ws[n++] = '/';
	ws[n] = '\0';
	x(!ffq_cmpz(ws, L"asd/"));
	ffmem_free(ws);
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

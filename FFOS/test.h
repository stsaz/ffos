/** ffos: test functions
2020, Simon Zolin
*/

#include <FFOS/mem.h>
#include <FFOS/error.h>
#include <FFOS/std.h>
#include <FFOS/string.h>
#include <ffbase/stringz.h>

#define FFTEST_FUNC
#define FFTEST_TIMECALL(f)  f

#define FFARRAY_FOREACH(static_array, it) \
	for (it = static_array;  it != static_array + FF_COUNT(static_array);  it++)

static inline void test_check(int ok, const char *expr, const char *file, ffuint line, const char *func)
{
	if (ok) {
		return;
	}

	ffstderr_fmt("FAIL: %s:%u: %s: %s\n"
		, file, line, func, expr);
	abort();
}

static inline void test_check_int_int(int ok, ffint64 i1, ffint64 i2, const char *file, ffuint line, const char *func)
{
	if (ok) {
		return;
	}

	ffstderr_fmt("FAIL: %s:%u: %s: %D != %D\n"
		, file, line, func
		, i1, i2);
	abort();
}

static inline void test_check_str_sz(int ok, ffsize slen, const char *s, const char *sz, const char *file, ffuint line, const char *func)
{
	if (ok) {
		return;
	}

	ffstderr_fmt("FAIL: %s:%u: %s: %*s != %s\n"
		, file, line, func
		, slen, s, sz);
	abort();
}

static inline void test_check_sys(int ok, const char *expr, const char *file, ffuint line, const char *func)
{
	if (ok) {
		return;
	}

	ffstderr_fmt("FAIL: %s:%u: %s: %s (%d %s)\n"
		, file, line, func, expr
		, (int)fferr_last(), fferr_strptr(fferr_last())
		);
	abort();
}

static inline void test_check_int_int_sys(int ok, ffint64 i1, ffint64 i2, const char *file, ffuint line, const char *func)
{
	if (ok) {
		return;
	}

	ffstderr_fmt("FAIL: %s:%u: %s: %D != %D (%d %s)\n"
		, file, line, func
		, i1, i2
		, (int)fferr_last(), fferr_strptr(fferr_last())
		);
	abort();
}

#define x(expr) \
	test_check(expr, #expr, __FILE__, __LINE__, __func__)

#define xieq(i1, i2) \
({ \
	ffint64 __i1 = (i1); \
	ffint64 __i2 = (i2); \
	test_check_int_int(__i1 == __i2, __i1, __i2, __FILE__, __LINE__, __func__); \
})

#define xseq(s, sz) \
({ \
	ffstr __s = *(s); \
	test_check_str_sz(ffstr_eqz(&__s, sz), __s.len, __s.ptr, sz, __FILE__, __LINE__, __func__); \
})

#define xsz(sz1, sz2) \
({ \
	test_check_str_sz(ffsz_eq(sz1, sz2), ffsz_len(sz1), sz1, sz2, __FILE__, __LINE__, __func__); \
})

/** Expect TRUE or die with system error */
#define x_sys(expr) \
({ \
	test_check_sys(expr, #expr, __FILE__, __LINE__, __func__); \
})

/** Expect equal integers or die with system error */
#define xint_sys(i1, i2) \
({ \
	ffint64 __i1 = (i1); \
	ffint64 __i2 = (i2); \
	test_check_int_int_sys(__i1 == __i2, __i1, __i2, __FILE__, __LINE__, __func__); \
})

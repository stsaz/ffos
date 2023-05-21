/** ffos: test functions
2020, Simon Zolin
*/

#include <FFOS/error.h>
#include <FFOS/std.h>
#include <FFOS/string.h>
#include <ffbase/stringz.h>

#define FFTEST_TIMECALL(f)  f
extern ffuint _ffos_checks_success;
extern ffuint _ffos_keep_running;

static inline void test_check(int ok, const char *file, ffuint line, const char *func, const char *fmt, ...)
{
	if (ok) {
		_ffos_checks_success++;
		return;
	}

	ffstr s = {};
	ffsize cap = 0;
	ffstr_growfmt(&s, &cap, "FAIL: %s:%u: %s: ", file, line, func);

	va_list va;
	va_start(va, fmt);
	ffstr_growfmtv(&s, &cap, fmt, va);
	va_end(va);

	ffstr_growaddchar(&s, &cap, '\n');
	ffstderr_write(s.ptr, s.len);
	ffstr_free(&s);

	if (!_ffos_keep_running)
		abort();
}

#define x(expr) \
	test_check(expr, __FILE__, __LINE__, __func__, "%s", #expr)

#define xieq(i1, i2) \
({ \
	ffint64 __i1 = (i1); \
	ffint64 __i2 = (i2); \
	test_check((__i1 == __i2), __FILE__, __LINE__, __func__, "%D != %D", __i1, __i2); \
})

#define xstr(str, sz) \
({ \
	ffstr __s = str; \
	test_check(ffstr_eqz(&__s, sz), __FILE__, __LINE__, __func__, "'%S' != '%s'", &__s, sz); \
})

#define xseq(str_ptr, sz)  xstr(*str_ptr, sz)

#define xsz(sz1, sz2) \
({ \
	test_check(ffsz_eq(sz1, sz2), __FILE__, __LINE__, __func__, "'%s' != '%s'", sz1, sz2); \
})

/** Expect TRUE or die with system error */
#define x_sys(expr) \
({ \
	test_check(expr, __FILE__, __LINE__, __func__, "%s: %E", #expr, fferr_last()); \
})

/** Expect equal integers or die with system error */
#define xint_sys(i1, i2) \
({ \
	ffint64 __i1 = (i1); \
	ffint64 __i2 = (i2); \
	test_check((__i1 == __i2), __FILE__, __LINE__, __func__, "%D != %D: %E", __i1, __i2, fferr_last()); \
})

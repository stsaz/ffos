/** ffos: error.h tester
2020, Simon Zolin
*/

#include <FFOS/string.h>
#include <FFOS/error.h>
#include <FFOS/test.h>

void test_error()
{
	fferr_set(1);
	xieq(1, fferr_last());

	char buf[64] = {};
	x(0 == fferr_str(-123456, buf, sizeof(buf)));
	fflog("fferr_str(-123456): %s", buf);
	x(buf[0] != '\0');

	x(0 != fferr_str(fferr_last(), buf, 1));
	x(buf[0] == '\0');

	x(0 != fferr_str(fferr_last(), NULL, 0));

	fferr_set(1);
	x(0 != fferr_str(fferr_last(), buf, 15));
#ifdef FF_WIN
	x(!strcmp("Incorrect func", buf) || !strcmp("Invalid functi", buf));
#else
	xsz("Operation not ", buf);
#endif

	x(0 == fferr_str(fferr_last(), buf, sizeof(buf)));

	const char *s = fferr_strptr(fferr_last());
#ifdef FF_WIN
	x(!strcmp("Incorrect function", s) || !strcmp("Invalid function", s));
#else
	xsz("Operation not permitted", s);
#endif
}

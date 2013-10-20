/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/time.h>
#include <FFOS/test.h>

#define x FFTEST_BOOL

int test_time()
{
	fftime t, t1;
	ffdtm dtm;

	FFTEST_FUNC;

	fftime_now(&t);
	x(t.s != 0);

	fftime_split(&dtm, &t, FFTIME_TZUTC);
	fftime_join(&t1, &dtm, FFTIME_TZUTC);
	t.mcs -= t.mcs % 1000;
	x(!fftime_cmp(&t, &t1));

	fftime_addms(&t1, 1);
	x(fftime_cmp(&t, &t1) < 0);

	fftime_addms(&t, 1000);
	x(fftime_cmp(&t, &t1) > 0);

	fftime_diff(&t1, &t);
	x(t.s == 0 && t.mcs == (1000 - 1) * 1000);

	return 0;
}

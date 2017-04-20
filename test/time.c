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

#if 0
	ffdtm dtmloc;
	fftime tloc;
	fftime_split(&dtmloc, &t, FFTIME_TZLOCAL);
	fftime_join(&tloc, &dtmloc, FFTIME_TZUTC);
	x(t.s + 3*60*60 == tloc.s);
	fftime_join(&tloc, &dtmloc, FFTIME_TZLOCAL);
	x(t.s == tloc.s);
#endif

	fftime_addms(&t1, 1);
	x(fftime_cmp(&t, &t1) < 0);

	fftime_addms(&t, 1000);
	x(fftime_cmp(&t, &t1) > 0);

	fftime_diff(&t1, &t);
	x(t.s == 0 && t.mcs == (1000 - 1) * 1000);

	x(fftime_chk(&dtm, 0));
	dtm.month = 2;
	dtm.day = 29;
	dtm.year = 2014;
	x(!fftime_chk(&dtm, 0));
	dtm.year = 2012;
	x(fftime_chk(&dtm, 0));

	return 0;
}

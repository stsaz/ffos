/**
Copyright (c) 2014 Simon Zolin
*/

#include <FFOS/test.h>
#include <FFOS/process.h>

fftest fftestobj;

int fftest_chk(int res, const char *file, uint line, const char *func, const char *sexp)
{
	if (res == 0)
		fftestobj.nfail++;
	fftestobj.nrun++;

	if ((fftestobj.flags & FFTEST_TRACE) || res == 0) {
		char msg[1024];
		fferr_str(fferr_last(), msg, sizeof(msg)-1);
		printf("%s\tat %s:%u in %s():\t%s\t(%d) %s\n"
			, (res == 0) ? "FAIL" : "OK", file, (int)line, func, sexp, (int)fferr_last(), msg);
	}

	if (res == 0 && (fftestobj.flags & FFTEST_FATALERR))
		ffps_exit(1);

	return res;
}

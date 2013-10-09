/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/types.h>
#include <FFOS/str.h>

FF_EXTN int test_types();
FF_EXTN int test_mem();
FF_EXTN int test_time();
FF_EXTN int test_file(const ffsyschar *tmpdir);
FF_EXTN int test_dir(const ffsyschar *tmpdir);

#ifdef FF_UNIX
#define TMP_PATH _S("/tmp")
#else
#define TMP_PATH _S(".")
#endif

static int test_all() {
	test_types();
	test_mem();
	test_time();
	test_file(TMP_PATH);
	test_dir(TMP_PATH);
	return 0;
}

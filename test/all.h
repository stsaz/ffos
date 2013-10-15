/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/types.h>
#include <FFOS/string.h>

FF_EXTN int test_types();
FF_EXTN int test_mem();
FF_EXTN int test_time();
FF_EXTN int test_file(const ffsyschar *tmpdir);
FF_EXTN int test_dir(const ffsyschar *tmpdir);
FF_EXTN int test_thd();
FF_EXTN int test_skt();

#ifdef FF_UNIX
#define TMP_PATH TEXT("/tmp")
#else
#define TMP_PATH TEXT(".")
#endif

FF_EXTN int test_all();

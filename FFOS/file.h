/**
File, file mapping, pipe, console I/O.
Copyright (c) 2013 Simon Zolin
*/

#pragma once

#include <FFOS/types.h>

#ifdef FF_WIN
#include <FFOS/win/file.h>
#include <FFOS/win/fmap.h>
#else
#include <FFOS/unix/file.h>
#include <FFOS/unix/fmap.h>
#endif

/** Get file time. */
FF_EXTN int fffile_time(fffd fd, fftime *last_write, fftime *last_access, fftime *creation);

/** Check whether the specified file exists. */
static FFINL ffbool fffile_exists(const char *filename) {
	int a = fffile_attrfn(filename);
	if (a == -1)
		return 0;
	return !fffile_isdir(a);
}

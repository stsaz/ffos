/**
Directory, path.
Copyright (c) 2013 Simon Zolin
*/

#pragma once

#include <FFOS/file.h>

#ifdef FF_UNIX
#include <FFOS/unix/dir.h>
#elif defined FF_WIN
#include <FFOS/win/dir.h>
#endif

/** Check whether the specified directory exists. */
static FFINL ffbool ffdir_exists(const char *filename) {
	int a = fffile_attrfn(filename);
	if (a == -1)
		return 0;
	return fffile_isdir(a);
}

/**
Directory, path.
Copyright (c) 2013 Simon Zolin
*/

#pragma once

#include <FFOS/file.h>

FF_EXTN int ffdir_rmakeq(ffsyschar *path, size_t off);

#ifdef FF_UNIX
#include <FFOS/unix/dir.h>
#elif defined FF_WIN
#include <FFOS/win/dir.h>
#endif

/** Create directory for a file. */
FF_EXTN int ffdir_make_path(char *fn);

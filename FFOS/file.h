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

/** Write constant string to a file. */
#define fffile_writecz(fd, csz)  fffile_write(fd, csz, sizeof(csz)-1)


enum FFSTD_ATTR {
	FFSTD_ECHO = 1,
	FFSTD_LINEINPUT = 2,
};

/** Set attribute on a terminal.
@attr: enum FFSTD_ATTR.
@val: enum FFSTD_ATTR.
*/
FF_EXTN int ffstd_attr(fffd fd, uint attr, uint val);

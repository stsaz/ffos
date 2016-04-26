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


enum FFKEY {
	FFKEY_VIRT = 1 << 31,
	FFKEY_UP,
	FFKEY_DOWN,
	FFKEY_RIGHT,
	FFKEY_LEFT,

	FFKEY_CTRL = 0x1 << 24,
	FFKEY_SHIFT = 0x2 << 24,
	FFKEY_ALT = 0x4 << 24,
	FFKEY_MODMASK = FFKEY_CTRL | FFKEY_SHIFT | FFKEY_ALT,
};

/** Parse key received from terminal.
Return enum FFKEY;  -1 on error. */
FF_EXTN int ffstd_key(const char *data, size_t *len);


enum FFSTD_ATTR {
	FFSTD_ECHO = 1,
	FFSTD_LINEINPUT = 2,
};

/** Set attribute on a terminal.
@attr: enum FFSTD_ATTR.
@val: enum FFSTD_ATTR.
*/
FF_EXTN int ffstd_attr(fffd fd, uint attr, uint val);

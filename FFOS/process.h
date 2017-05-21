/** Process, dynamic library, system config.
Copyright (c) 2013 Simon Zolin
*/

#pragma once

#include <FFOS/types.h>

#ifdef FF_UNIX
#include <FFOS/unix/ps.h>

#elif defined FF_WIN
#include <FFOS/win/ps.h>
#endif

/** Create a copy of the current process in background.
Return child process descriptor (parent);  0 (child);  -1 on error. */
FF_EXTN fffd ffps_createself_bg(const char *arg);

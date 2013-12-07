/** Process, dynamic library
Copyright (c) 2013 Simon Zolin
*/

#pragma once

#include <FFOS/types.h>

#ifdef FF_UNIX
#include <FFOS/unix/ps.h>

#elif defined FF_WIN
#include <FFOS/win/ps.h>
#endif

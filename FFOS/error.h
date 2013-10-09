/**
System error.
Copyright (c) 2013 Simon Zolin
*/

#pragma once

#include <FFOS/types.h>

#ifdef FF_UNIX
#include <FFOS/unix/error.h>
#elif defined FF_WIN
#include <FFOS/win/error.h>
#endif

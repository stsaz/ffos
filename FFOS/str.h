/**
String.
Copyright (c) 2013 Simon Zolin
*/

#pragma once

#include <FFOS/types.h>

#define FFSLEN(s)  (FFCNT(s) - 1)

#define FFSTR(s)  (char*)(s), FFSLEN(s)

#define FFSTRQ(s)  (ffsyschar*)_S(s), FFSLEN(s)

#ifdef FF_UNIX
#include <FFOS/unix/str.h>
#else
#include <FFOS/win/str.h>
#endif

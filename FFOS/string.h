/**
String.
Copyright (c) 2013 Simon Zolin
*/

#pragma once

#include <FFOS/types.h>

#define FFSLEN(s)  (FFCNT(s) - 1)

#define FFSTR(s)  (char*)(s), FFSLEN(s)

#define FFSTRQ(s)  (ffsyschar*)TEXT(s), FFSLEN(s)

#define FFCRLF "\r\n"

#ifdef FF_UNIX
#include <FFOS/unix/str.h>
#else
#include <FFOS/win/str.h>
#endif

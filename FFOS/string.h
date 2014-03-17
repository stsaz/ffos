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
#include <wchar.h>

#else
#include <FFOS/win/str.h>
#endif

#define ffq_alloc(n)  ffmem_talloc(ffsyschar, (n))

/** Convert wide string to UTF-8. */
FF_EXTN size_t ff_wtou(char *dst, size_t dst_cap, const wchar_t *src, size_t srclen, int flags);

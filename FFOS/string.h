/**
String.
Copyright (c) 2013 Simon Zolin
*/

#pragma once

#include <FFOS/types.h>

/** Enclose multi-line text into double quotes.
Double quotes inside FFS_QUOTE() are auto-escaped.
Note: excessive whitespace is merged.

Example:
	FFS_QUOTE(
		select *
		from table
	)
is translated into "select * from table". */
#define FFS_QUOTE(...)  #__VA_ARGS__

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

#define ffq_alloc(n)  ffmem_allocT(n, ffsyschar)

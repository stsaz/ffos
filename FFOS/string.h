/**
String.
Copyright (c) 2013 Simon Zolin
*/

#pragma once

#include <FFOS/mem.h>
static int fferr_str(int code, char *buffer, ffsize cap);
#include <ffbase/string.h>

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

#include <string.h>
#include <wchar.h>

#if !defined TEXT
	#define TEXT(s)  s
#endif

#define FF_NEWLN  "\n"

#define ffq_len  strlen
#define ffq_cmpz  strcmp
#define ffq_icmpz  strcasecmp
#define ffq_icmpnz  strncasecmp
#define ffq_cpy2  strcpy
#define ffq_cat2  strcat

#else // Windows:

#include <ffbase/unicode.h>

#define FF_NEWLN  "\r\n"

#if defined FF_MSVC || defined FF_MINGW
	#define strncasecmp  _strnicmp
	#define wcsncasecmp  _wcsnicmp
	#define wcscasecmp  _wcsicmp
#endif

#define ffq_len  wcslen
#define ffq_cmpz  wcscmp
#define ffq_icmpz  wcscasecmp
#define ffq_icmpnz  wcsncasecmp
#define ffq_cpy2  wcscpy
#define ffq_cat2  wcscat

/** Convert UTF-8 to wide string.  Allocate memory, if needed.
dst: optional
dstlen: capacity of 'dst'.
len: if -1 then 's' is treated as null-terminated and 'dst' will also be null-terminated.
Return wide-string.  If not equal to 'dst' then free with ffmem_free().
  'dstlen' is set to the number of written characters. */
FF_EXTN WCHAR* ffs_utow(WCHAR *dst, size_t *dstlen, const char *s, size_t len);

/** Convert UTF-8 to wide string.
len: if -1 then 'src' is treated as null-terminated and 'dst' will also be null-terminated. */
static inline ffsize ff_utow(wchar_t *dst, ffsize cap, const char *src, ffsize len, int flags)
{
	FF_ASSERT(flags == 0);
	ffssize r;
	if (len == (ffsize)-1)
		r = ffsz_utow(dst, cap, src);
	else {
		r = ffutf8_to_utf16((char*)dst, cap * sizeof(wchar_t), src, len, FFUNICODE_UTF16LE);
		r /= 2;
	}
	return (r >= 0) ? r : 0;
}

/** Convert wide string to UTF-8. */
static inline ffsize ff_wtou(char *dst, ffsize cap, const wchar_t *src, ffsize len, int flags)
{
	FF_ASSERT(flags == 0);
	ffssize r;
	if (len == (ffsize)-1)
		r = ffsz_wtou(dst, cap, src);
	else
		r = ffs_wtou(dst, cap, src, len);
	return (r >= 0) ? r : 0;
}

/** Convert UTF-8 to UTF-16LE
Try to use the buffer supplied by user,
 but if dst==NULL or there's not enough space - allocate a new buffer.
Return a static or newly allocated buffer (free with ffmem_free())
  NULL on error */
static inline wchar_t* ffsz_alloc_buf_utow(wchar_t *dst, ffsize cap_wchars, const char *sz)
{
	if (dst != NULL) {
		if (0 < ffsz_utow(dst, cap_wchars, sz))
			return dst;
	}
	return ffsz_alloc_utow(sz);
}

static inline wchar_t* ffs_alloc_buf_utow(wchar_t *dst, ffsize *cap_wchars, const char *s, ffsize len)
{
	ffssize r;
	if (dst != NULL) {
		r = ffutf8_to_utf16((char*)dst, *cap_wchars * 2, s, len, FFUNICODE_UTF16LE);
		if (r >= 0) {
			*cap_wchars = r / 2;
			return dst;
		}
	}

	r = ffutf8_to_utf16(NULL, 0, s, len, FFUNICODE_UTF16LE);

	wchar_t *w;
	if (NULL == (w = (wchar_t*)ffmem_alloc(r)))
		return NULL;

	r = ffutf8_to_utf16((char*)w, r, s, len, FFUNICODE_UTF16LE);
	if (r < 0) {
		ffmem_free(w);
		return NULL;
	}

	*cap_wchars = r / 2;
	return w;
}

#endif

#define ffq_alloc(n)  ffmem_allocT(n, ffsyschar)

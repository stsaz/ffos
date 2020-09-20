/**
String.
Copyright (c) 2013 Simon Zolin
*/

#pragma once

#include <FFOS/base.h>
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

#define FFSTR(s)  (char*)(s), FFS_LEN(s)

#define FFCRLF "\r\n"

#ifdef FF_UNIX

#include <string.h>
#include <wchar.h>

#define FFSTRQ(s)  s, FFS_LEN(s)

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
#define FFSTRQ(s)  L##s, FFS_LEN(s)

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

/** Convert UTF-8 to wide string.
len: if -1 then 'src' is treated as null-terminated and 'dst' will also be null-terminated. */
static inline ffsize ff_utow(wchar_t *dst, ffsize cap_wchars, const char *src, ffsize len, int flags)
{
	FF_ASSERT(flags == 0);
	ffssize r;
	if (len == (ffsize)-1)
		r = ffsz_utow(dst, cap_wchars, src);
	else {
		r = ffutf8_to_utf16((char*)dst, cap_wchars * sizeof(wchar_t), src, len, FFUNICODE_UTF16LE);
		r /= 2;
	}
	return (r >= 0) ? r : 0;
}

static inline ffssize _ffs_utow(wchar_t *dst, ffsize cap_wchars, const char *src, ffsize len)
{
	ffssize r;
	r = ffutf8_to_utf16((char*)dst, cap_wchars * sizeof(wchar_t), src, len, FFUNICODE_UTF16LE);
	if (r <= 0)
		return r;
	return r / 2;
}

/** Convert UTF-8 to wide string.  Allocate memory, if needed.
dst: optional
dstlen: capacity of 'dst'.
len: if -1 then 's' is treated as null-terminated and 'dst' will also be null-terminated.
Return wide-string.  If not equal to 'dst' then free with ffmem_free().
  'dstlen' is set to the number of written characters. */
static inline wchar_t* ffs_utow(wchar_t *dst, size_t *dstlen, const char *s, size_t len)
{
	size_t wlen;

	if (dst != NULL) {
		wlen = ff_utow(dst, *dstlen, s, len, 0);
		if (wlen != 0 || len == 0)
			goto done;
	}

	//not enough space in the provided buffer.  Allocate a new one.
	wlen = (len == (size_t)-1) ? strlen(s) + 1 : len + 1;
	dst = ffmem_allocT(wlen, wchar_t);
	if (dst == NULL)
		return NULL;

	wlen = ff_utow(dst, wlen, s, len, 0);
	if (wlen == 0) {
		ffmem_free(dst);
		return NULL;
	}

done:
	if (dstlen != NULL)
		*dstlen = wlen;
	return dst;
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

/**
add_chars: N of additional characters to allocate the space for
Return allocated buffer;  add_chars: N of characters written */
static inline wchar_t* ffs_alloc_utow_addcap(const char *s, ffsize len, ffsize *add_chars)
{
	ffssize n;
	if (0 > (n = _ffs_utow(NULL, 0, s, len)))
		return NULL;
	n += *add_chars;

	wchar_t *w;
	if (NULL == (w = (wchar_t*)ffmem_alloc(n * sizeof(wchar_t))))
		return NULL;

	*add_chars = _ffs_utow(w, n, s, len);
	return w;
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

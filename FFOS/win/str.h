/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/mem.h>
#include <ffbase/unicode.h>
#ifdef __CYGWIN__
#include <wchar.h>
#endif

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
@dst: optional
@dstlen: capacity of 'dst'.
@len: if -1 then 's' is treated as null-terminated and 'dst' will also be null-terminated.
Return wide-string.  If not equal to 'dst' then free with ffmem_free().
 'dstlen' is set to the number of written characters.*/
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

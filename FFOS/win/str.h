/**
Copyright (c) 2013 Simon Zolin
*/

#ifdef __CYGWIN__
#include <wchar.h>
#endif

#ifndef _S
	#define _S(s)  L##s
#endif

#define FF_NEWLN  "\r\n"

#ifdef FF_MSVC
	#define strncasecmp  _strnicmp
	#define wcsncasecmp  _wcsnicmp
#endif

#define ffq_len  wcslen
#define ffq_cmp2  wcscmp
#define ffq_icmp2  wcsicmp
#define ffq_icmp  wcsncasecmp
#define ffq_cpy2  wcscpy
#define ffq_cat2  wcscat

/** Convert UTF-8 to wide string. */
FF_EXTN size_t ff_utow(WCHAR *dst, size_t dst_cap, const char *src, size_t srclen, int flags);

/** Convert wide string to UTF-8. */
FF_EXTN size_t ff_wtou(char *dst, size_t dst_cap, const WCHAR *src, size_t srclen, int flags);

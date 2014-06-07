/**
Copyright (c) 2013 Simon Zolin
*/

#ifdef __CYGWIN__
#include <wchar.h>
#endif

#define FF_NEWLN  "\r\n"

#ifdef FF_MSVC
	#define strncasecmp  _strnicmp
	#define wcsncasecmp  _wcsnicmp
#endif

#define ffq_len  wcslen
#define ffq_cmpz  wcscmp
#define ffq_icmpz  _wcsicmp
#define ffq_icmpnz  wcsncasecmp
#define ffq_cpy2  wcscpy
#define ffq_cat2  wcscat

/** Convert UTF-8 to wide string. */
FF_EXTN size_t ff_utow(WCHAR *dst, size_t dst_cap, const char *src, size_t srclen, int flags);

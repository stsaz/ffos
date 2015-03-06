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
	#define wcscasecmp  _wcsicmp
#endif

#define ffq_len  wcslen
#define ffq_cmpz  wcscmp
#define ffq_icmpz  wcscasecmp
#define ffq_icmpnz  wcsncasecmp
#define ffq_cpy2  wcscpy
#define ffq_cat2  wcscat

/** Convert UTF-8 to wide string.
@srclen: if -1 then 'src' is treated as null-terminated and 'dst' will also be null-terminated. */
FF_EXTN size_t ff_utow(WCHAR *dst, size_t dst_cap, const char *src, size_t srclen, int flags);

/** Convert UTF-8 to wide string.  Allocate memory, if needed.
@dst: optional
@dstlen: capacity of 'dst'.
@len: if -1 then 's' is treated as null-terminated and 'dst' will also be null-terminated.
Return wide-string.  If not equal to 'dst' then free with ffmem_free().
 'dstlen' is set to the number of written characters.*/
FF_EXTN WCHAR* ffs_utow(WCHAR *dst, size_t *dstlen, const char *s, size_t len);

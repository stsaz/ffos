/**
Copyright (c) 2013 Simon Zolin
*/

#include <string.h>
#include <errno.h>

/** Get system error message. */
#ifdef FF_LINUX
FF_EXTN int fferr_str(int code, char *dst, size_t dst_cap);
#else
static FFINL int fferr_str(int code, char *dst, size_t dst_cap) {
	if (0 == dst_cap)
		return 0;
	dst[0] = '\0';
	strerror_r(code, dst, dst_cap);
	return strlen(dst);
}
#endif

/** Get pointer to a system error message. */
#define fferr_strp  strerror

/** Get last system error code. */
#define fferr_last()  errno

/** Set last system error code. */
#define fferr_set(code)  errno = (code)

#define fferr_again(code)  ((code) == EAGAIN || (code) == EWOULDBLOCK)

#define fferr_nofile(code)  ((code) == ENOENT)

/** Return TRUE if reached system limit of opened file descriptors. */
#define fferr_fdlim(code)  ((code) == EMFILE || (code) == ENFILE)

/** ffos: system error
2020, Simon Zolin
*/

/*
fferr_last
fferr_set
Checks:
	fferr_again
	fferr_exist
	fferr_notexist
	fferr_fdlimit
fferr_str fferr_strptr
*/

#pragma once

#include <FFOS/base.h>

#ifdef FF_WIN

#include <FFOS/string.h>
#include <ffbase/stringz.h>

#define FFERR_TIMEOUT  WSAETIMEDOUT

static inline int fferr_last()
{
	return GetLastError();
}

static inline void fferr_set(int code)
{
	SetLastError(code);
}

static inline int fferr_again(int code)
{
	return code == WSAEWOULDBLOCK;
}

static inline int fferr_exist(int code)
{
	return code == ERROR_FILE_EXISTS || code == ERROR_ALREADY_EXISTS;
}

static inline int fferr_notexist(int code)
{
	return code == ERROR_FILE_NOT_FOUND || code == ERROR_PATH_NOT_FOUND
		|| code == ERROR_NOT_READY || code == ERROR_INVALID_NAME;
}

static inline int fferr_fdlimit(int code)
{
	(void)code;
	return 0;
}

static inline int fferr_str(int code, char *buffer, ffsize cap)
{
	if (cap == 0)
		return -1;

	wchar_t ws[256];
	const int f = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK;
	int n = FormatMessageW(f, 0, code, 0, ws, sizeof(ws), 0);
	if (n == 0) {
		ffsz_copyz(buffer, cap, "unknown error");
		return 0;
	}

	// cut the trailing ". "
	if (n > 2 && ws[n - 2] == '.' && ws[n - 1] == ' ') {
		n -= 2;
		ws[n] = '\0';
	}

	int r = ffs_wtou(buffer, cap, ws, n + 1);
	if (r <= 0) {
		r = ffs_wtou(NULL, 0, ws, n + 1);
		if ((ffuint)r > cap)
			buffer[cap - 1] = '\0';
		return -1;
	}

	return 0;
}

FF_EXTERN char _fferr_buffer[1024];

static inline const char* fferr_strptr(int code)
{
	fferr_str(code, _fferr_buffer, sizeof(_fferr_buffer));
	return _fferr_buffer;
}

#else // UNIX:

static int fferr_str(int code, char *buffer, ffsize cap);
#include <ffbase/stringz.h>
#include <errno.h>

#define FFERR_TIMEOUT  ETIMEDOUT

static inline int fferr_last()
{
	return errno;
}

static inline void fferr_set(int code)
{
	errno = code;
}

static inline int fferr_again(int code)
{
	return code == EAGAIN || code == EWOULDBLOCK;
}

static inline int fferr_exist(int code)
{
	return code == EEXIST;
}

static inline int fferr_notexist(int code)
{
	return code == ENOENT;
}

static inline int fferr_fdlimit(int code)
{
	return code == EMFILE || code == ENFILE;
}

static inline int fferr_str(int code, char *buffer, ffsize cap)
{
	if (cap == 0)
		return -1;

#if defined _GNU_SOURCE && (!defined FF_ANDROID || __ANDROID_API__ >= 23)

	const char *r = strerror_r(code, buffer, cap);
	if (r != buffer) {
		if (cap == ffsz_copyz(buffer, cap, r))
			return -1;
	}
	return 0;

#else

	if (ERANGE == strerror_r(code, buffer, cap))
		return -1;
	return 0;

#endif
}

static inline const char* fferr_strptr(int code)
{
	return strerror(code);
}

#endif


/** Get last system error code */
static int fferr_last();

/** Set last system error code */
static void fferr_set(int code);

/** Return TRUE if operation would block */
static inline int fferr_again(int code);

/** Return TRUE if file exists */
static inline int fferr_exist(int code);

/** Return TRUE if file doesn't exist */
static inline int fferr_notexist(int code);

/** Return TRUE if reached system limit of opened file descriptors */
static inline int fferr_fdlimit(int code);

/** Get error message
buffer: buffer for output NULL-terminated string
Return 0 on success
  !=0 on error */
static int fferr_str(int code, char *buffer, ffsize cap);

/** Get pointer to a system error message */
static const char* fferr_strptr(int code);

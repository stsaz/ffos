/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/string.h>


#define fferr_last  GetLastError

#define fferr_set  SetLastError

FF_EXTN int fferr_strq(int code, ffsyschar *dst, size_t dst_cap);

static FFINL int fferr_str(int code, char *dst, size_t dst_cap)
{
	ffsyschar w[255];
	fferr_strq(code, w, FFCNT(w));
	if (0 == ff_wtou(dst, dst_cap, w, -1, 0))
		return -1;
	return 0;
}

FF_EXTN const char* fferr_strp(int code);

#define fferr_again(code)  (code == WSAEWOULDBLOCK)

#define fferr_exist(code)  ((code) == ERROR_FILE_EXISTS || (code) == ERROR_ALREADY_EXISTS)

#define fferr_fdlim(code)  (0)

/**
return 0 if b = TRUE or (b = FALSE and IO_PENDING)
return -1 if b = FALSE and !IO_PENDING */
#define fferr_ioret(b) (((b) || GetLastError() == ERROR_IO_PENDING) ? 0 : -1)

static FFINL ffbool fferr_nofile(int e) {
	return e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND
		|| e == ERROR_NOT_READY || e == ERROR_INVALID_NAME;
}

#ifdef FF_MINGW
#undef EINVAL
#undef EEXIST
#undef EOVERFLOW
#undef ENOSPC
#undef EBADF
#undef ENOMEM
#undef EACCES
#undef ENOTEMPTY
#undef ETIMEDOUT
#undef EAGAIN
#undef ECANCELED
#undef EINTR
#undef ENOENT
#endif

enum FF_ERRORS {
	EINVAL = ERROR_INVALID_PARAMETER
	, EEXIST = ERROR_ALREADY_EXISTS //ERROR_FILE_EXISTS
	, EOVERFLOW = ERROR_INVALID_DATA
	, ENOSPC = ERROR_DISK_FULL
	, EBADF = ERROR_INVALID_HANDLE
	, ENOMEM = ERROR_NOT_ENOUGH_MEMORY
	, EACCES = ERROR_ACCESS_DENIED
	, ENOTEMPTY = ERROR_DIR_NOT_EMPTY
	, ETIMEDOUT = WSAETIMEDOUT
	, EAGAIN = WSAEWOULDBLOCK
	, ECANCELED = ERROR_OPERATION_ABORTED
	, EINTR = WAIT_TIMEOUT
	,
	ENOENT = ERROR_FILE_NOT_FOUND,
};

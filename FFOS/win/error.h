/**
Copyright (c) 2013 Simon Zolin
*/

#define fferr_last  GetLastError

#define fferr_set  SetLastError

FF_EXTN int fferr_str(int code, ffsyschar *dst, size_t dst_cap);

#define fferr_again(code)  (code == WSAEWOULDBLOCK)

/**
return 0 if b = TRUE or (b = FALSE and IO_PENDING)
return -1 if b = FALSE and !IO_PENDING */
static FFINL int fferr_ioret(BOOL b) {
	return ((b || GetLastError() == ERROR_IO_PENDING) ? 0 : -1);
}

static ffbool fferr_nofile(int e) {
	return e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND
		|| e == ERROR_NOT_READY || e == ERROR_INVALID_NAME;
}

enum FF_ERRORS {
	EINVAL = ERROR_INVALID_PARAMETER
	, EEXIST = ERROR_ALREADY_EXISTS //ERROR_FILE_EXISTS
	, EOVERFLOW = ERROR_INVALID_DATA
	, ENOSPC = ERROR_DISK_FULL
	, EBADF = ERROR_INVALID_HANDLE
	, ENOMEM = ERROR_NOT_ENOUGH_MEMORY
	, EACCES = ERROR_ACCESS_DENIED
	, ENOTEMPTY = ERROR_DIR_NOT_EMPTY
};

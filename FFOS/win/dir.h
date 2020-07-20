/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/file.h>


#define FFDIR_USER_CONFIG  "%APPDATA%"


enum {
	ENOMOREFILES = ERROR_NO_MORE_FILES
	/** Case-insensitive file system. */
	, FFPATH_ICASE = 1
};

#define FFPATH_SLASH '\\'

static FFINL ffbool ffpath_slash(ffsyschar c) {
	return c == '/' || c == '\\';
}

FF_EXTN ffbool ffpath_abs(const char *path, size_t len);

/**
Return TRUE if long name. */
static FFINL ffbool ffpath_islong(const ffsyschar *fnw) {
	ffsyschar fnwLong[FF_MAXPATH];
	uint n = GetLongPathNameW(fnw, fnwLong, FFCNT(fnwLong));
	if (n == 0)
		return 0;
	if (0 != ffq_icmpz(fnwLong, fnw)) {
		fferr_set(ERROR_INVALID_NAME);
		return 0;
	}
	return 1;
}


typedef struct ffpathinfo {
	uint f_bsize;
} ffpathinfo;

FF_EXTN int ffpath_infoinit(const char *path, ffpathinfo *st);

enum FFPATH_INFO {
	FFPATH_BSIZE
};

static FFINL uint64 ffpath_info(ffpathinfo *st, uint name)
{
	switch (name) {
	case FFPATH_BSIZE:
		return st->f_bsize;
	}

	return 0;
}

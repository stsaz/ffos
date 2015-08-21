/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/string.h>

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


#define ffdir_makeq(name)  (0 == CreateDirectory(name, NULL))

FF_EXTN int ffdir_make(const char *name);

FF_EXTN int ffdir_rmake(const char *path, size_t off);

static FFINL int ffdir_rmq(const ffsyschar *name) {
	return 0 == RemoveDirectory(name);
}

FF_EXTN int ffdir_rm(const char *name);


typedef fffd ffdir;
typedef WIN32_FIND_DATA ffdir_einfo;

typedef struct ffdirentry {
	ffdir_einfo info;
	uint namelen : 31
		, next : 1;
} ffdirentry;

FF_EXTN ffdir ffdir_openq(ffsyschar *path, size_t cap, ffdirentry *ent);

FF_EXTN ffdir ffdir_open(char *path, size_t cap, ffdirentry *ent);

FF_EXTN int ffdir_read(ffdir dir, ffdirentry *ent);

#define ffdir_entryname(ent)  (ent)->info.cFileName

#define ffdir_entryinfo(ent)  (&(ent)->info)

static FFINL int ffdir_close(ffdir dir) {
	return 0 == FindClose(dir);
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

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

static FFINL int ffdir_make(const char *name) {
	ffsyschar wname[FF_MAXPATH];
	if (0 == ff_utow(wname, FFCNT(wname), name, -1, 0))
		return -1;
	return ffdir_makeq(wname);
}

static FFINL int ffdir_rmq(const ffsyschar *name) {
	return 0 == RemoveDirectory(name);
}

static FFINL int ffdir_rm(const char *name) {
	ffsyschar wname[FF_MAXPATH];
	if (0 == ff_utow(wname, FFCNT(wname), name, -1, 0))
		return -1;
	return ffdir_rmq(wname);
}


typedef fffd ffdir;

typedef struct ffdirentry {
	fffileinfo info;
	uint namelen : 31
		, next : 1;
} ffdirentry;

FF_EXTN ffdir ffdir_openq(ffsyschar *path, size_t cap, ffdirentry *ent);

static FFINL ffdir ffdir_open(char *path, size_t cap, ffdirentry *ent) {
	ffsyschar wname[FF_MAXPATH];
	if (0 == ff_utow(wname, FFCNT(wname), path, -1, 0))
		return 0;
	return ffdir_openq(wname, FF_MAXPATH, ent);
}

FF_EXTN int ffdir_read(ffdir dir, ffdirentry *ent);

#define ffdir_entryname(ent)  (ent)->info.data.cFileName

#define ffdir_entryinfo(ent)  (&(ent)->info)

static FFINL int ffdir_close(ffdir dir) {
	return 0 == FindClose(dir);
}

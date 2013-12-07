/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/string.h>
#include <FFOS/file.h>

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
static FFINL ffbool ffpath_islong(const ffsyschar *fnw, size_t sz) {
	ffsyschar fnwLong[FF_MAXPATH];
	uint n = GetLongPathNameW(fnw, fnwLong, FFCNT(fnwLong));
	if (n == sz && !ffq_icmpnz(fnwLong, fnw, sz))
		return 1;
	return 0;
}


#define ffdir_make(name)  (0 == CreateDirectory(name, NULL))

#define ffdir_rm(name)  (0 == RemoveDirectory(name))


typedef fffd ffdir;

typedef struct {
	fffileinfo info;
	uint namelen : 31
		, next : 1;
} ffdirentry;

FF_EXTN ffdir ffdir_open(ffsyschar *path, size_t cap, ffdirentry *ent);

FF_EXTN int ffdir_read(ffdir dir, ffdirentry *ent);

#define ffdir_entryname(ent)  (ent)->info.data.cFileName

#define ffdir_entryinfo(ent)  (&(ent)->info)

#define ffdir_close(dir)  (0 == FindClose(dir))

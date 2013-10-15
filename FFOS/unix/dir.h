/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/file.h>

#include <string.h>
#include <errno.h>
#include <dirent.h>

enum {
	ENOMOREFILES = 0
	, FFPATH_ICASE = 0
};

/** File system native path separator. */
#define FFPATH_SLASH  '/'

/** Return TRUE if character is a valid path separator. */
#define ffpath_slash(c)  ((c) == FFPATH_SLASH)

/** Check whether the specified path is absolute. */
static FFINL ffbool ffpath_abs(const ffsyschar *path, size_t len) {
	return len >= 1 && path[0] == '/';
}


/** Create a new directory. */
#define ffdir_make(name)  mkdir(name, 0777)

/** Delete the directory. */
#define ffdir_rm(name)  rmdir(name)


typedef DIR * ffdir;

typedef struct {
	fffileinfo info;
	size_t pathlen;
	ffsyschar *path;
	size_t pathcap;
	struct dirent de;
	uint namelen;
} ffdirentry;

/** Open directory reader.
Return 0 on error.
'path' should have at least FF_MAXFN characters of free space. */
static FFINL ffdir ffdir_open(ffsyschar *path, size_t pathcap, ffdirentry *ent) {
	ent->pathlen = strlen(path);
	ent->path = path;
	ent->pathcap = pathcap;
	return opendir(path);
}

/** Read directory.
Return 0 on success.
Fail with ENOMOREFILES if there are no more files. */
static FFINL int ffdir_read(ffdir dir, ffdirentry *ent) {
	struct dirent *d;
	errno = ENOMOREFILES;
	if (0 == readdir_r(dir, &ent->de, &d) && d != NULL) {
		ent->namelen = (int)strlen(ent->de.d_name);
		return 0;
	}
	return -1;
}

/** Get entry name. */
#define ffdir_entryname(ent)  (ent)->de.d_name

/** Get file information.
Return NULL on error.
Fail with EOVERFLOW if there is not enough space in 'path'. */
FF_EXTN fffileinfo * ffdir_entryinfo(ffdirentry *ent);

/** Close directory reader. */
#define ffdir_close  closedir

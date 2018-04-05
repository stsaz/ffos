/**
Copyright (c) 2013 Simon Zolin
*/

#include <string.h>
#include <errno.h>
#include <dirent.h>

#if defined FF_BSD || defined FF_APPLE
#include <sys/mount.h>
#else
#include <sys/vfs.h>
#endif


#define FFDIR_USER_CONFIG  "$HOME/.config"


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

/** Recursively create directories.
@off: length of the path that already exists. */
#define ffdir_rmake(path, off)  ffdir_rmakeq(path, off)

/** Delete the directory. */
#define ffdir_rm(name)  rmdir(name)


typedef DIR * ffdir;
typedef fffileinfo ffdir_einfo;

typedef struct ffdirentry {
	ffdir_einfo info;
	size_t pathlen;
	char *path;
	size_t pathcap;
	struct dirent de;
	uint namelen;
} ffdirentry;

/** Open directory reader.
Return 0 on error.
'path' should have at least FF_MAXFN characters of free space. */
static FFINL ffdir ffdir_open(char *path, size_t pathcap, ffdirentry *ent) {
	ent->pathlen = strlen(path);
	ent->path = path;
	ent->pathcap = pathcap;
	return opendir(path);
}

/** Read directory.
Return 0 on success.
Fail with ENOMOREFILES if there are no more files. */
FF_EXTN int ffdir_read(ffdir dir, ffdirentry *ent);

/** Get entry name. */
#define ffdir_entryname(ent)  (ent)->de.d_name

/** Get file information.
Return NULL on error.
Fail with EOVERFLOW if there is not enough space in 'path'. */
FF_EXTN ffdir_einfo * ffdir_entryinfo(ffdirentry *ent);

/** Close directory reader. */
#define ffdir_close  closedir


#define ffdir_makeq ffdir_make


typedef struct statfs ffpathinfo;

/** Initialize statfs structure. */
#define ffpath_infoinit(path, st)  statfs(path, st)

enum FFPATH_INFO {
	FFPATH_BSIZE
};

/** Get file-system parameter.
@name: enum FFPATH_INFO. */
static FFINL uint64 ffpath_info(ffpathinfo *st, uint name)
{
	switch (name) {
	case FFPATH_BSIZE:
		return st->f_bsize;
	}

	return 0;
}

FF_EXTN const char* _ffpath_real(char *name, size_t cap, const char *argv0);

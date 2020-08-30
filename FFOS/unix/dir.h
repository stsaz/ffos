/**
Copyright (c) 2013 Simon Zolin
*/

#include <string.h>
#include <errno.h>

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

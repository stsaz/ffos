/**
Copyright (c) 2017 Simon Zolin
*/

#pragma once


/** Open or create a file.
flags: O_*.
Linux: may fail with EINVAL when O_DIRECT is used.
Return FF_BADFD on error. */
static FFINL fffd fffile_open(const char *filename, int flags)
{
	fffd f;
	flags |= O_LARGEFILE;
	f = open(filename, flags, 0666);
	if (f == FF_BADFD && fferr_last() == EPERM && (flags & O_NOATIME)) {
		flags &= ~O_NOATIME;
		f = open(filename, flags, 0666);
	}
	return f;
}

enum FFFILE_SEEK {
	FFFILE_SEEK_BEGIN = SEEK_SET,
	FFFILE_SEEK_CURRENT = SEEK_SET,
	FFFILE_SEEK_END = SEEK_END,
};

/** Reposition file offset.
method: enum FFFILE_SEEK
Return -1 on error. */
#define fffile_seek(fd, pos, method)  lseek64(fd, pos, method)

#ifdef FF_LINUX_MAINLINE
static FFINL int fffile_readahead(fffd fd, size_t size)
{
	(void)size;
	int er = posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
	if (er != 0) {
		errno = er;
		return 1;
	}
	return 0;
}
#endif

/** Return last-write time from fileinfo. */
static FFINL fftime fffile_infomtime(const fffileinfo *fi)
{
	fftime t;
	fftime_fromtimespec(&t, &fi->st_mtim);
	return t;
}

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

/** Reposition file offset.
Use SEEK_*
Return -1 on error. */
#define fffile_seek  lseek64

#ifdef FF_LINUX_MAINLINE
static FFINL int fffile_readahead(fffd fd, size_t size)
{
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

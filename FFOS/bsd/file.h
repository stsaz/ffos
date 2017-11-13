/**
Copyright (c) 2017 Simon Zolin
*/

#pragma once


enum {
	O_NOATIME = 0,
#if defined FF_APPLE
	O_DIRECT = 0,
#endif
};

#define fffile_open(filename, flags) \
	open(filename, flags, 0666)

#define fffile_seek  lseek

#define fffile_readahead(fd, size)  fcntl(fd, F_READAHEAD, size)

#if defined FF_APPLE
static FFINL fftime fffile_infomtime(const fffileinfo *fi)
{
	fftime t;
	fftime_fromtimespec(&t, &fi->st_mtimespec);
	return t;
}

#else
static FFINL fftime fffile_infomtime(const fffileinfo *fi)
{
	fftime t;
	fftime_fromtimespec(&t, &fi->st_mtim);
	return t;
}
#endif

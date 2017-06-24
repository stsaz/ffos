/**
Copyright (c) 2017 Simon Zolin
*/

#pragma once


enum {
	O_NOATIME = 0,
};

#define fffile_open(filename, flags) \
	open(filename, flags, 0666)

#define fffile_seek  lseek

#define fffile_readahead(fd, size)  fcntl(fd, F_READAHEAD, size)

static FFINL fftime fffile_infomtime(const fffileinfo *fi)
{
	fftime t;
	fftime_fromtimespec(&t, &fi->st_mtim);
	return t;
}

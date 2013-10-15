/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/time.h>

#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>

enum {
	FF_MAXPATH = 4096
	, FF_MAXFN = 256
	, FF_MAXFN_UTF8 = FF_MAXFN
};

enum FFFILE_OPEN {
	FFO_OPEN = 0
	, FFO_CREATENEW = O_CREAT | O_EXCL
	, FFO_CREATE = O_CREAT | O_TRUNC
	, FFO_TRUNC = O_TRUNC
	, FFO_APPEND = O_APPEND | O_CREAT

	, O_DIR = 0

#ifdef FF_BSD
	, O_NOATIME = 0
#endif
};

/** Open or create a file.
flags: O_*.
Return FF_BADFD on error. */
static FFINL fffd fffile_open(const ffsyschar *filename, int flags) {
#ifdef FF_LINUX
	flags |= O_LARGEFILE;
#endif
	return open(filename, flags, 0666);
}


/** Read from a file descriptor.
Return -1 on error. */
static FFINL ssize_t fffile_read(fffd fd, void *out, size_t size) {
	return read(fd, out, size);
}

/** Write to a file descriptor.
Return -1 on error. */
static FFINL ssize_t fffile_write(fffd fd, const void *in, size_t size) {
	return write(fd, in, size);
}

#ifdef FF_BSD
#define fffile_seek  lseek
#else
/** Reposition file offset.
Use SEEK_*
Return -1 on error. */
#define fffile_seek  lseek64

#endif

/** Truncate a file to a specified length. */
#define fffile_trunc  ftruncate

/** Set file non-blocking mode. */
FF_EXTN int fffile_nblock(fffd fd, int nblock);

/** Duplicate a file descriptor.
Return FF_BADFD on error. */
#define fffile_dup  dup

/** Close a file descriptor. */
#define fffile_close  close

/** Get file size.
Return -1 on error. */
static FFINL int64 fffile_size(fffd fd) {
	struct stat s;
	if (0 != fstat(fd, &s))
		return -1;
	return s.st_size;
}

/** Get file attributes.
Return -1 on error. */
static FFINL int fffile_attr(fffd fd) {
	struct stat s;
	if (0 != fstat(fd, &s))
		return -1;
	return s.st_mode;
}

/** Get file attributes by file name.
Return -1 on error. */
static FFINL int fffile_attrfn(const ffsyschar *filename) {
	struct stat s;
	if (0 != stat(filename, &s))
		return -1;
	return s.st_mode;
}

/** Check whether directory flag is set in file attributes. */
#define fffile_isdir(file_attr)  (((file_attr) & S_IFMT) == S_IFDIR)

/** Set file attributes. */
#define fffile_attrset  fchmod

/** Set file attributes by file name. */
#define fffile_attrsetfn  chmod


/** File information. */
typedef struct stat fffileinfo;

/** Get file status by file descriptor. */
#define fffile_info  fstat

/** Get file status by name. */
#define fffile_infofn  stat

/** Return last-write time from fileinfo. */
static FFINL fftime fffile_infotimew(const fffileinfo *fi) {
	fftime t;
	t.s = fi->st_mtime;
	t.mcs = 0;
	return t;
}

/** Return file size from fileinfo. */
#define fffile_infosize(fi)  (fi)->st_size

/** Return file attributes from fileinfo. */
#define fffile_infoattr(fi)  (fi)->st_mode


/** Change the name or location of a file. */
#define fffile_rename  rename

/** Create a hard link to the file. */
#define fffile_hardlink  link

/** Delete a name and possibly the file it refers to. */
#define fffile_rm  unlink


enum FFSTD {
	ffstdin = 0
	, ffstdout = 1
	, ffstderr = 2
};

/** Read console input. */
#define ffstd_read  fffile_read

/** Print to console. */
#define ffstd_write  fffile_write


/** Create pipe. */
static FFINL int ffpipe_create(fffd *rd, fffd *wr) {
	fffd fd[2];
	if (0 != pipe(fd))
		return -1;
	*rd = fd[0];
	*wr = fd[1];
	return 0;
}

/** Close a pipe. */
#define ffpipe_close  close

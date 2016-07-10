/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/time.h>

#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <errno.h>


enum {
	FF_MAXPATH = 4096
	, FF_MAXFN = 256
};

enum FFFILE_OPEN {
	FFO_CREATENEW = O_CREAT | O_EXCL
	, FFO_APPEND = O_APPEND | O_CREAT

	, FFO_NODOSNAME = 0

#ifdef FF_BSD
	, O_NOATIME = 0
#endif
};

/** Open or create a file.
flags: O_*.
Return FF_BADFD on error. */
static FFINL fffd fffile_open(const char *filename, int flags) {
#ifdef FF_LINUX
	flags |= O_LARGEFILE;
#endif
	return open(filename, flags, 0666);
}

static FFINL fffd fffile_createtemp(const char *filename, int flags) {
	fffd fd = fffile_open(filename, FFO_CREATENEW | flags);
	if (fd != FF_BADFD)
		unlink(filename);
	return fd;
}

#define fffile_createtempq  fffile_createtemp

#ifdef FF_LINUX
/** Try to open a file with O_DIRECT.
Linux: If opened without O_DIRECT, AIO operations will always block. */
static FFINL fffd fffile_opendirect(const char *filename, uint flags)
{
	fffd f = fffile_open(filename, flags | O_DIRECT);
	if (f == FF_BADFD && errno == EINVAL)
		f = fffile_open(filename, flags);
	return f;
}

#else //bsd:
#define fffile_opendirect(filename, flags)  fffile_open(filename, flags | O_DIRECT)
#endif

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

/** File I/O at a specific offset. */
#define fffile_pwrite(fd, buf, size, off)  pwrite(fd, buf, size, off)
#define fffile_pread(fd, buf, size, off)  pread(fd, buf, size, off)

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
static FFINL int fffile_nblock(fffd fd, int nblock) {
	return ioctl(fd, FIONBIO, &nblock);
}

#ifdef FF_BSD
#define fffile_readahead(fd, size)  fcntl(fd, F_READAHEAD, size)

#else
static FFINL int fffile_readahead(fffd fd, size_t size) {
	int er = posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
	if (er != 0) {
		errno = er;
		return 1;
	}
	return 0;
}
#endif

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

/** Check whether directory flag is set in file attributes. */
#define fffile_isdir(file_attr)  (((file_attr) & S_IFMT) == S_IFDIR)

/** Set file attributes. */
#define fffile_attrset  fchmod

/** Set file attributes by file name. */
#define fffile_attrsetfn  chmod

/** Change ownership of a file. */
#define fffile_chown(fd, uid, gid)  fchown(fd, uid, gid)


/** File information. */
typedef struct stat fffileinfo;

/** Get file status by file descriptor. */
#define fffile_info  fstat

/** Get file status by name. */
#define fffile_infofn  stat

/** Return last-write time from fileinfo. */
static FFINL fftime fffile_infomtime(const fffileinfo *fi) {
	fftime t;
#ifdef FF_BSD
	fftime_fromtimespec(&t, &fi->st_mtim);
#else
	t.s = fi->st_mtime;
	t.mcs = 0;
#endif
	return t;
}

/** Return file size from fileinfo. */
#define fffile_infosize(fi)  (fi)->st_size

/** Return file attributes from fileinfo. */
#define fffile_infoattr(fi)  (fi)->st_mode

typedef ino_t fffileid;

/** Get file ID. */
#define fffile_infoid(fi)  (fi)->st_ino

static FFINL int fffile_settime(fffd fd, const fftime *last_write)
{
	struct timeval tv[2];
	tv[0].tv_sec = last_write->s;
	tv[0].tv_usec = last_write->mcs;
	tv[1].tv_sec = last_write->s;
	tv[1].tv_usec = last_write->mcs;
	return futimes(fd, tv);
}


/** Change the name or location of a file. */
#define fffile_rename  rename

/** Create a hard link to the file. */
#define fffile_hardlink  link

/** Delete a name and possibly the file it refers to. */
#define fffile_rm  unlink


enum FFSTD {
	ffstdin = STDIN_FILENO
	, ffstdout = STDOUT_FILENO
	, ffstderr = STDERR_FILENO
};

/** Read console input. */
#define ffstd_read  fffile_read

/** Print to console. */
#define ffstd_write  fffile_write

typedef struct ffstd_ev {
	char buf[8];

	char *data;
	size_t datalen;
} ffstd_ev;

/** Read keyboard event from terminal.
Return 1 on success;  0 if queue is empty;  -1 on error. */
FF_EXTN int ffstd_event(fffd fd, ffstd_ev *ev);


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


#define fffile_openq  fffile_open
#define fffile_renameq  fffile_rename
#define fffile_rmq  fffile_rm

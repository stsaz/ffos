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


/** File information. */
typedef struct stat fffileinfo;

#ifdef FF_LINUX
#include <FFOS/linux/file.h>
#else
#include <FFOS/bsd/file.h>
#endif


enum {
	FF_MAXPATH = 4096
	, FF_MAXFN = 256
};

enum FFFILE_OPEN {
	FFO_CREATENEW = O_CREAT | O_EXCL
	, FFO_APPEND = O_APPEND | O_CREAT

	, FFO_NODOSNAME = 0
	, FFO_NONBLOCK = O_NONBLOCK
};


static FFINL fffd fffile_createtemp(const char *filename, int flags) {
	fffd fd = fffile_open(filename, FFO_CREATENEW | flags);
	if (fd != FF_BADFD)
		unlink(filename);
	return fd;
}

#define fffile_createtempq  fffile_createtemp

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

/** Truncate a file to a specified length.
Windows: un-aligned truncate on a file with O_DIRECT fails with ERROR_INVALID_PARAMETER
 due to un-aligned seeking request. */
static FFINL int fffile_trunc(fffd fd, uint64 len)
{
	return ftruncate(fd, len);
}

/** Set file non-blocking mode. */
static FFINL int fffile_nblock(fffd fd, int nblock) {
	return ioctl(fd, FIONBIO, &nblock);
}

/** Duplicate a file descriptor.
Return FF_BADFD on error. */
#define fffile_dup(fd)  dup(fd)

/** Close a file descriptor. */
#define fffile_close(fd)  close(fd)

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
#define fffile_attrset(fd, mode)  fchmod(fd, mode)

/** Set file attributes by file name. */
#define fffile_attrsetfn(fn, mode)  chmod(fn, mode)

/** Change ownership of a file. */
#define fffile_chown(fd, uid, gid)  fchown(fd, uid, gid)


/** Get file status by file descriptor. */
#define fffile_info(fd, fi)  fstat(fd, fi)

/** Get file status by name. */
#define fffile_infofn(fn, fi)  stat(fn, fi)

/** Return file size from fileinfo. */
#define fffile_infosize(fi)  (fi)->st_size

/** Return file attributes from fileinfo. */
#define fffile_infoattr(fi)  (fi)->st_mode

typedef ino_t fffileid;

/** Get file ID. */
#define fffile_infoid(fi)  (fi)->st_ino

FF_EXTN int fffile_settime(fffd fd, const fftime *last_write);
FF_EXTN int fffile_settimefn(const char *fn, const fftime *last_write);

/** Change the name or location of a file. */
#define fffile_rename(oldpath, newpath)  rename(oldpath, newpath)

/** Create a hard link to the file. */
#define fffile_hardlink(oldpath, newpath)  link(oldpath, newpath)

/** Create a symbolic link to the file. */
#define fffile_symlink(target, linkpath)  symlink(target, linkpath)

/** Delete a name and possibly the file it refers to. */
#define fffile_rm(fn)  unlink(fn)


enum FFSTD {
	ffstdin = STDIN_FILENO
	, ffstdout = STDOUT_FILENO
	, ffstderr = STDERR_FILENO
};

/** Read console input. */
#define ffstd_read  fffile_read
#define ffstd_fread(fd, buf, cap)  fffile_read(fd, buf, cap)

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

#define ffterm_detach()


// UNNAMED/NAMED PIPES

/** Create pipe.
flags: FFO_NONBLOCK */
static inline int ffpipe_create2(fffd *rd, fffd *wr, uint flags)
{
	fffd fd[2];
	if (0 != pipe2(fd, flags))
		return -1;
	*rd = fd[0];
	*wr = fd[1];
	return 0;
}

static FFINL int ffpipe_create(fffd *rd, fffd *wr) {
	return ffpipe_create2(rd, wr, 0);
}

/** Set non-blocking mode on a pipe descriptor. */
#define ffpipe_nblock(fd, nblock)  fffile_nblock(fd, nblock)

/** Create named pipe.
@name: UNIX: file name to be used for UNIX socket.
@name: Windows: \\.\pipe\NAME.
@flags: SOCK_NONBLOCK */
FF_EXTN fffd ffpipe_create_named(const char *name, uint flags);

/** Close a pipe. */
#define ffpipe_close(fd)  close(fd)

/** Close a peer pipe fd returned by ffaio_pipe_accept(). */
#define ffpipe_peer_close(fd)  close(fd)

FF_EXTN fffd ffpipe_connect(const char *name);

/** Close a client pipe fd returned by ffpipe_connect(). */
#define ffpipe_client_close(fd)  close(fd)

/** Read from a pipe descriptor. */
#define ffpipe_read(fd, buf, cap)  fffile_read(fd, buf, cap)


#define fffile_openq  fffile_open
#define fffile_renameq  fffile_rename
#define fffile_rmq  fffile_rm

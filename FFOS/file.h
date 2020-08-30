/** ffos: file, file mapping, pipe, console I/O
2020, Simon Zolin
*/

/*
Get properties:
	fffile_info_path fffile_info
	fffile_size fffileinfo_size
	fffileinfo_mtime
	fffileinfo_attr fffile_isdir
	fffileinfo_id
	fffileinfo_owner
	fffile_exists
Set properties:
	fffile_set_mtime_path fffile_set_mtime
	fffile_set_attr_path fffile_set_attr
	fffile_set_owner
Name/links:
	fffile_rename
	fffile_remove
	fffile_hardlink fffile_symlink
I/O:
	fffile_open fffile_createtemp fffile_dup fffile_close
	fffile_nonblock
	fffile_seek
	fffile_readahead
	fffile_write fffile_writeat
	fffile_read fffile_readat
	fffile_trunc
*/

#pragma once

#include <FFOS/time.h>

// TTTT SSS RWXRWXRWX
enum FFFILE_UNIX_FILEATTR {
	// Type:
	FFFILE_UNIX_TYPEMASK = 0170000,
	FFFILE_UNIX_FIFO = 0010000,
	FFFILE_UNIX_CHAR = 0020000,
	FFFILE_UNIX_DIR = 0040000,
	FFFILE_UNIX_BLOCK = 0060000,
	FFFILE_UNIX_REG = 0100000,
	FFFILE_UNIX_LINK = 0120000,
	FFFILE_UNIX_SOCKET = 0140000,
};

enum FFFILE_WIN_FILEATTR {
	FFFILE_WIN_READONLY = 1,
	FFFILE_WIN_HIDDEN = 2,
	FFFILE_WIN_SYSTEM = 4,
	FFFILE_WIN_DIR = 0x10,
	FFFILE_WIN_ARCHIVE = 0x20,
};


#ifdef FF_WIN

#include <FFOS/string.h>

#define FFFILE_NULL  INVALID_HANDLE_VALUE
#define FFERR_FILENOTFOUND  ERROR_FILE_NOT_FOUND
#define FFERR_FILEEXISTS  ERROR_FILE_EXISTS

typedef BY_HANDLE_FILE_INFORMATION fffileinfo;

static inline int fffile_info_path(const char *name, fffileinfo *fi)
{
	wchar_t ws[256], *w;
	if (NULL == (w = ffsz_alloc_buf_utow(ws, FF_COUNT(ws), name)))
		return -1;

	WIN32_FILE_ATTRIBUTE_DATA fad;
	int r;
	if (0 != (r = !GetFileAttributesEx(w, GetFileExInfoStandard, &fad)))
		goto end;

	fi->dwFileAttributes = fad.dwFileAttributes;
	fi->ftCreationTime = fad.ftCreationTime;
	fi->ftLastAccessTime = fad.ftLastAccessTime;
	fi->ftLastWriteTime = fad.ftLastWriteTime;
	fi->nFileSizeHigh = fad.nFileSizeHigh;
	fi->nFileSizeLow = fad.nFileSizeLow;
	fi->nFileIndexHigh = fi->nFileIndexLow = 0;

end:
	if (w != ws)
		ffmem_free(w);
	return r;
}

static inline int fffile_info(fffd fd, fffileinfo *fi)
{
	return !GetFileInformationByHandle(fd, fi);
}

static inline ffint64 fffile_size(fffd fd)
{
	ffuint64 size = 0;
	if (!GetFileSizeEx(fd, (LARGE_INTEGER*)&size))
		return -1;
	return size;
}

static inline ffuint64 fffileinfo_size(const fffileinfo *fi)
{
	return ((ffuint64)fi->nFileSizeHigh << 32) | fi->nFileSizeLow;
}

static inline fftime fffileinfo_mtime(const fffileinfo *fi)
{
	return fftime_from_winftime((fftime_winftime*)&fi->ftLastWriteTime);
}

static inline ffuint fffileinfo_attr(const fffileinfo *fi)
{
	return fi->dwFileAttributes;
}

static inline int fffile_isdir(ffuint file_attr)
{
	return ((file_attr & FILE_ATTRIBUTE_DIRECTORY) != 0);
}

static inline ffuint64 fffileinfo_id(const fffileinfo *fi)
{
	return ((ffuint64)fi->nFileIndexHigh << 32) | fi->nFileIndexLow;
}


static inline int fffile_set_mtime(fffd fd, const fftime *last_write)
{
	FILETIME ft;
	fftime_to_winftime(last_write, (fftime_winftime*)&ft);
	return !SetFileTime(fd, NULL, NULL, &ft);
}

static inline int fffile_set_attr_path(const char *name, ffuint attr)
{
	wchar_t ws[256], *w;
	if (NULL == (w = ffsz_alloc_buf_utow(ws, FF_COUNT(ws), name)))
		return -1;

	int r = !SetFileAttributesW(w, attr);

	if (w != ws)
		ffmem_free(w);
	return r;
}

#if FF_WIN >= 0x0600
static inline int fffile_set_attr(fffd fd, ffuint attr)
{
	FILE_BASIC_INFO i = {};
	i.FileAttributes = attr;
	return !SetFileInformationByHandle(fd, FileBasicInfo, &i, sizeof(FILE_BASIC_INFO));
}
#endif

static inline int fffile_rename(const char *oldpath, const char *newpath)
{
	wchar_t ws_old[256], *w_old;
	wchar_t ws_new[256], *w_new = ws_new;
	if (NULL == (w_old = ffsz_alloc_buf_utow(ws_old, FF_COUNT(ws_old), oldpath)))
		return -1;
	if (NULL == (w_new = ffsz_alloc_buf_utow(ws_new, FF_COUNT(ws_new), newpath)))
		return -1;

	int r = !MoveFileExW(w_old, w_new, MOVEFILE_REPLACE_EXISTING);

	if (w_old != ws_old)
		ffmem_free(w_old);
	if (w_new != ws_new)
		ffmem_free(w_new);
	return r;
}

static inline int fffile_remove(const char *name)
{
	wchar_t ws[256], *w;
	if (NULL == (w = ffsz_alloc_buf_utow(ws, FF_COUNT(ws), name)))
		return -1;

	int r = !DeleteFileW(w);

	if (r != 0 && GetLastError() == ERROR_ACCESS_DENIED) {

		int attr = GetFileAttributesW(w);
		if (attr == -1
			|| !(attr & FFFILE_WIN_READONLY)
			|| !SetFileAttributesW(w, attr & ~FFFILE_WIN_READONLY))
			goto end;

		r = !DeleteFileW(w);
	}

end:
	if (w != ws)
		ffmem_free(w);
	return r;
}

static inline int fffile_hardlink(const char *oldpath, const char *newpath)
{
	wchar_t ws_old[256], *w_old;
	wchar_t ws_new[256], *w_new = ws_new;
	if (NULL == (w_old = ffsz_alloc_buf_utow(ws_old, FF_COUNT(ws_old), oldpath)))
		return -1;
	if (NULL == (w_new = ffsz_alloc_buf_utow(ws_new, FF_COUNT(ws_new), newpath)))
		return -1;

	int r = !CreateHardLinkW(w_new, w_old, NULL);

	if (w_old != ws_old)
		ffmem_free(w_old);
	if (w_new != ws_new)
		ffmem_free(w_new);
	return r;
}


/* Mask:
......0f  Windows file attributes
......f0  Creation
....0f00  Access
....f000  FF flags
ffff0000  Windows file open flags */
enum FFFILE_OPEN {
	// Creation:
	FFFILE_CREATE = OPEN_ALWAYS << 4,
	FFFILE_CREATENEW = CREATE_NEW << 4,

	// Access:
	FFFILE_READONLY = GENERIC_READ >> 20,
	FFFILE_WRITEONLY = GENERIC_WRITE >> 20,
	FFFILE_READWRITE = (GENERIC_READ | GENERIC_WRITE) >> 20,

	// Open flags:
	FFFILE_TRUNCATE = 0x1000,
	FFFILE_NODOSNAME = 0x2000,

	// Flags:
	FFFILE_APPEND = 0x4000,
	FFFILE_NONBLOCK = 0,
	FFFILE_NOATIME = 0,
	FFFILE_DIRECT = FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED,
};

static inline fffd fffile_open(const char *name, ffuint flags)
{
	ffuint creation = (flags & 0xf0) >> 4;
	if ((flags & FFFILE_TRUNCATE) && creation != FFFILE_CREATENEW)
		creation = TRUNCATE_EXISTING;
	else if (creation == 0)
		creation = OPEN_EXISTING;

	ffuint access = (flags & 0x0f00) << 20;
	if (flags & FFFILE_APPEND) {
		access = 0;
		if (flags & FFFILE_READONLY)
			access |= FILE_GENERIC_READ;
		if (flags & FFFILE_WRITEONLY)
			access |= (FILE_GENERIC_WRITE & ~FILE_WRITE_DATA);
	}

	const ffuint share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;

	ffuint f = FILE_ATTRIBUTE_NORMAL
		| (flags & 0x0f) /*user-specified FILE_ATTRIBUTE_... flags*/
		| (flags & 0xffff0000) /*user-specified FILE_FLAG_... flags*/
		| FILE_FLAG_BACKUP_SEMANTICS /*allow opening directories*/;

	wchar_t ws[256], *w;
	wchar_t longname[256], *wlong = longname;
	if (NULL == (w = ffsz_alloc_buf_utow(ws, FF_COUNT(ws), name)))
		return FFFILE_NULL;

	fffd fd = FFFILE_NULL;
	if (flags & FFFILE_NODOSNAME) {
		ffuint n = GetLongPathNameW(w, NULL, 0);
		if (n > FF_COUNT(longname))
			wlong = (wchar_t*)ffmem_alloc(n * sizeof(wchar_t));

		n = GetLongPathNameW(w, wlong, n);
		if (n == 0)
			goto end;

		if (0 != ffq_icmpz(longname, w)) {
			SetLastError(ERROR_INVALID_NAME);
			goto end;
		}
	}

	fd = CreateFileW(w, access, share, NULL, creation, f, NULL);

	if (fd == FFFILE_NULL && GetLastError() == ERROR_FILE_NOT_FOUND
		&& (flags & (FFFILE_TRUNCATE | 0xf0)) == (FFFILE_TRUNCATE | FFFILE_CREATE))
		fd = CreateFileW(w, access, share, NULL, CREATE_NEW, f, NULL);

end:
	if (w != ws)
		ffmem_free(w);
	if (wlong != longname)
		ffmem_free(wlong);
	return fd;

}

static inline fffd fffile_createtemp(const char *name, ffuint flags)
{
	return fffile_open(name, FFFILE_CREATENEW | FILE_FLAG_DELETE_ON_CLOSE | (flags & ~0x0f));
}

static inline fffd fffile_dup(fffd fd)
{
	fffd r = INVALID_HANDLE_VALUE;
	HANDLE ps = GetCurrentProcess();
	DuplicateHandle(ps, fd, ps, &r, 0, 0, DUPLICATE_SAME_ACCESS);
	return r;
}

static inline int fffile_close(fffd fd)
{
	return !CloseHandle(fd);
}

enum FFFILE_SEEK {
	FFFILE_SEEK_BEGIN = FILE_BEGIN,
	FFFILE_SEEK_CURRENT = FILE_CURRENT,
	FFFILE_SEEK_END = FILE_END,
};

static inline ffint64 fffile_seek(fffd fd, ffuint64 pos, int method)
{
	ffint64 r;
	if (!SetFilePointerEx(fd, *(LARGE_INTEGER*)&pos, (LARGE_INTEGER*)&r, method))
		return -1;
	return r;
}

static inline int fffile_readahead(fffd fd, ffint64 size)
{
	(void)fd; (void)size;
	SetLastError(ERROR_NOT_SUPPORTED);
	return -1;
}

static inline ffssize fffile_write(fffd fd, const void *data, ffsize size)
{
	DWORD wr;
	if (!WriteFile(fd, data, ffmin(size, 0xffffffff), &wr, 0))
		return -1;
	return wr;
}

static inline ffssize fffile_writeat(fffd fd, const void *data, ffsize size, ffuint64 off)
{
	OVERLAPPED ovl = {};
	ovl.Offset = (ffuint)off;
	ovl.OffsetHigh = (ffuint)(off >> 32);
	DWORD wr;
	if (!WriteFile(fd, data, ffmin(size, 0xffffffff), &wr, &ovl))
		return -1;
	return wr;
}

static inline ffssize fffile_read(fffd fd, void *buf, ffsize size)
{
	DWORD rd;
	if (!ReadFile(fd, buf, ffmin(size, 0xffffffff), &rd, 0))
		return -1;
	return rd;
}

static inline ffssize fffile_readat(fffd fd, void *buf, ffsize size, ffuint64 off)
{
	OVERLAPPED ovl = {};
	ovl.Offset = (ffuint)off;
	ovl.OffsetHigh = (ffuint)(off >> 32);
	DWORD rd;
	if (!ReadFile(fd, buf, ffmin(size, 0xffffffff), &rd, &ovl)) {
		if (GetLastError() == ERROR_HANDLE_EOF)
			return 0;
		return -1;
	}
	return rd;
}

static inline int fffile_trunc(fffd fd, ffuint64 len)
{
	ffint64 pos = fffile_seek(fd, 0, FILE_CURRENT); // get current offset
	if (pos < 0
		|| 0 > fffile_seek(fd, len, FILE_BEGIN)) // seek to the specified offset
		return -1;

	int r = !SetEndOfFile(fd);

	if (0 > fffile_seek(fd, pos, FILE_BEGIN)) // restore current offset
		r = -1;
	return r;
}

static inline int fffile_set_mtime_path(const char *name, const fftime *last_write)
{
	fffd fd;
	if (FFFILE_NULL == (fd = fffile_open(name, FFFILE_WRITEONLY)))
		return -1;
	int r = fffile_set_mtime(fd, last_write);
	fffile_close(fd);
	return r;
}

#else // UNIX:

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <errno.h>

#define FFFILE_NULL  (-1)
#define FFERR_FILENOTFOUND  ENOENT
#define FFERR_FILEEXISTS  EEXIST

#ifndef fffileinfo // may be defined in FFOS/dir.h
	#define fffileinfo  struct stat
#endif

static inline int fffile_info_path(const char *name, fffileinfo *fi)
{
	return stat(name, fi);
}

static inline int fffile_info(fffd fd, fffileinfo *fi)
{
	return fstat(fd, fi);
}

static inline ffint64 fffile_size(fffd fd)
{
	struct stat s;
	if (0 != fstat(fd, &s))
		return -1;
	return s.st_size;
}

static inline ffuint64 fffileinfo_size(const fffileinfo *fi)
{
	return fi->st_size;
}

static inline fftime fffileinfo_mtime(const fffileinfo *fi)
{
	fftime t;

#ifdef FF_APPLE
	fftime_fromtimespec(&t, &fi->st_mtimespec);

#else
	fftime_fromtimespec(&t, &fi->st_mtim);
#endif

	return t;
}

static inline ffuint fffileinfo_attr(const fffileinfo *fi)
{
	return fi->st_mode;
}

static inline int fffile_isdir(ffuint file_attr)
{
	return ((file_attr & S_IFMT) == S_IFDIR);
}

static inline ffuint64 fffileinfo_id(const fffileinfo *fi)
{
	return fi->st_ino;
}

static inline ffuint fffileinfo_owner(const fffileinfo *fi)
{
	return fi->st_uid;
}

static inline int fffile_set_mtime_path(const char *name, const fftime *last_write)
{
#ifdef FF_LINUX
	struct timespec ts[2];
	fftime_to_timespec(last_write, &ts[0]);
	fftime_to_timespec(last_write, &ts[1]);
	return utimensat(AT_FDCWD, name, ts, 0);

#else
	struct timeval tv[2];
	fftime_to_timeval(last_write, &tv[0]);
	fftime_to_timeval(last_write, &tv[1]);
	return utimes(name, tv);
#endif
}

static inline int fffile_set_mtime(fffd fd, const fftime *last_write)
{
#if defined FF_LINUX && !defined FF_ANDROID
	struct timespec ts[2];
	fftime_to_timespec(last_write, &ts[0]);
	fftime_to_timespec(last_write, &ts[1]);
	return futimens(fd, ts);

#else
	struct timeval tv[2];
	fftime_to_timeval(last_write, &tv[0]);
	fftime_to_timeval(last_write, &tv[1]);
	return futimes(fd, tv);
#endif
}

static inline int fffile_set_attr_path(const char *name, ffuint mode)
{
	return chmod(name, mode);
}

static inline int fffile_set_attr(fffd fd, ffuint mode)
{
	return fchmod(fd, mode);
}

static inline int fffile_set_owner(fffd fd, ffuint uid, ffuint gid)
{
	return fchown(fd, uid, gid);
}

static inline int fffile_rename(const char *oldpath, const char *newpath)
{
	return rename(oldpath, newpath);
}

static inline int fffile_remove(const char *name)
{
	return unlink(name);
}

static inline int fffile_hardlink(const char *oldpath, const char *newpath)
{
	return link(oldpath, newpath);
}

static inline int fffile_symlink(const char *target, const char *linkpath)
{
	return symlink(target, linkpath);
}

enum FFFILE_OPEN {
	// Creation:
	FFFILE_CREATE = O_CREAT,
	FFFILE_CREATENEW = O_CREAT | O_EXCL,

	// Access:
	FFFILE_READONLY = O_RDONLY,
	FFFILE_WRITEONLY = O_WRONLY,
	FFFILE_READWRITE = O_RDWR,

	// Open flags:
	FFFILE_TRUNCATE = O_TRUNC,
	FFFILE_NODOSNAME = 0,

	// Flags:
	FFFILE_APPEND = O_APPEND,
	FFFILE_NONBLOCK = O_NONBLOCK,

#ifdef FF_LINUX
	FFFILE_NOATIME = O_NOATIME,
#else
	FFFILE_NOATIME = 0,
#endif

#ifdef FF_APPLE
	FFFILE_DIRECT = 0,
#else
	FFFILE_DIRECT = O_DIRECT,
#endif
};

static inline fffd fffile_open(const char *name, ffuint flags)
{
	ffuint mode = 0666;

#ifdef FF_LINUX
	flags |= O_LARGEFILE;
	fffd fd = open(name, flags, mode);

	if (fd == FFFILE_NULL && errno == EPERM
		&& (flags & O_NOATIME)) {
		flags &= ~O_NOATIME;
		fd = open(name, flags, mode);
	}

#else
	fffd fd = open(name, flags, mode);
#endif

	return fd;
}

static inline fffd fffile_createtemp(const char *name, ffuint flags)
{
	fffd fd = fffile_open(name, FFFILE_CREATENEW | flags);
	if (fd != FFFILE_NULL)
		unlink(name);
	return fd;
}

static inline fffd fffile_dup(fffd fd)
{
	return dup(fd);
}

static inline int fffile_close(fffd fd)
{
	return close(fd);
}

enum FFFILE_SEEK {
	FFFILE_SEEK_BEGIN = SEEK_SET,
	FFFILE_SEEK_CURRENT = SEEK_CUR,
	FFFILE_SEEK_END = SEEK_END,
};

static inline ffint64 fffile_seek(fffd fd, ffuint64 pos, int method)
{
#ifdef FF_LINUX
	return lseek64(fd, pos, method);

#else
	return lseek(fd, pos, method);
#endif
}

static inline int fffile_readahead(fffd fd, ffint64 size)
{
#if defined FF_ANDROID
	errno = ENOSYS;
	return -1;

#elif defined FF_LINUX
	int f = POSIX_FADV_SEQUENTIAL;
	if (size == -1) {
		size = 0;
		f = POSIX_FADV_NORMAL;
	}

	int r = posix_fadvise(fd, 0, size, f);
	if (r != 0) {
		errno = r;
		return 1;
	}
	return 0;

#elif defined FF_APPLE
	return fcntl(fd, F_RDAHEAD, size);

#else
	return fcntl(fd, F_READAHEAD, size);
#endif
}

static inline ffssize fffile_write(fffd fd, const void *data, ffsize size)
{
	return write(fd, data, size);
}

static inline ffssize fffile_writeat(fffd fd, const void *data, ffsize size, ffuint64 off)
{
	return pwrite(fd, data, size, off);
}

static inline ffssize fffile_read(fffd fd, void *buf, ffsize size)
{
	return read(fd, buf, size);
}

static inline ffssize fffile_readat(fffd fd, void *buf, ffsize size, ffuint64 off)
{
	return pread(fd, buf, size, off);
}

static inline int fffile_trunc(fffd fd, ffuint64 len)
{
	return ftruncate(fd, len);
}

static inline int fffile_nonblock(fffd fd, int nonblock)
{
	return ioctl(fd, FIONBIO, &nonblock);
}

#endif


/** Get file status by name
Windows: fffileinfo_id() won't work
Return !=0 on error */
static int fffile_info_path(const char *name, fffileinfo *fi);

/** Get file status by file descriptor
Return !=0 on error */
static int fffile_info(fffd fd, fffileinfo *fi);

/** Get file size by file descriptor
Return <0 on error */
static ffint64 fffile_size(fffd fd);

/** Get file size from fffileinfo */
static ffuint64 fffileinfo_size(const fffileinfo *fi);

/** Get last-write time from fileinfo */
static fftime fffileinfo_mtime(const fffileinfo *fi);

/** Get file attributes from fffileinfo
Return OS-specific value */
static ffuint fffileinfo_attr(const fffileinfo *fi);

/** Check whether directory flag is set in file attributes */
static int fffile_isdir(ffuint file_attr);

/** Get file ID */
static ffuint64 fffileinfo_id(const fffileinfo *fi);

#ifndef FF_WIN
/** Get User ID of owner
Windows: not implemented */
static ffuint fffileinfo_owner(const fffileinfo *fi);
#endif

/** Check if file path exists */
static inline ffbool fffile_exists(const char *name)
{
	fffileinfo fi;
	return 0 == fffile_info_path(name, &fi);
}


/** Set file last-modification time by name */
static int fffile_set_mtime_path(const char *name, const fftime *last_write);

/** Set file last-modification time by descriptor */
static int fffile_set_mtime(fffd fd, const fftime *last_write);

/** Set file attributes by file name */
static int fffile_set_attr_path(const char *name, ffuint mode);

#if !defined FF_WIN || FF_WIN >= 0x0600
/** Set file attributes */
static int fffile_set_attr(fffd fd, ffuint mode);
#endif

#ifndef FF_WIN
/** Change ownership of a file
Windows: not implemented */
static int fffile_set_owner(fffd fd, ffuint uid, ffuint gid);
#endif


/** Change the name or location of a file */
static int fffile_rename(const char *oldpath, const char *newpath);

/** Delete a name and possibly the file it refers to */
static int fffile_remove(const char *name);

/** Create a hard link to the file */
static int fffile_hardlink(const char *oldpath, const char *newpath);

#ifndef FF_WIN
/** Create a symbolic link to the file
Windows: not implemented */
static int fffile_symlink(const char *target, const char *linkpath);
#endif


/** Open or create a file
Close with fffile_close()
flags:
  Creation:
    0: open existing
    FFFILE_CREATE: create new or open existing
    FFFILE_CREATENEW: create new and fail if exists
  Access:
    FFFILE_READONLY: read-only
    FFFILE_WRITEONLY: write-only
    FFFILE_READWRITE: read/write
  Flags:
    FFFILE_APPEND: automatically seek to EOF before each write()
    FFFILE_TRUNCATE: truncate to length 0
    FFFILE_NODOSNAME: Windows: opening a file with 8.3 name will fail
    Any OS-specific flag:
      UNIX: O_...
      Windows: FILE_FLAG_... | FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM
Return FFFILE_NULL on error
  Linux: may fail with errno=EINVAL with O_DIRECT flag */
static fffd fffile_open(const char *name, ffuint flags);

/** Create temporary file
Fail if file exists
Close with fffile_close()
Return FFFILE_NULL on error */
static fffd fffile_createtemp(const char *name, ffuint flags);

/** Duplicate a file descriptor
Return FFFILE_NULL on error */
static fffd fffile_dup(fffd fd);

/** Close a file descriptor
Return !=0 on error, i.e. file isn't written completely */
static int fffile_close(fffd fd);

#ifndef FF_WIN
/** Set file non-blocking mode
Windows: not implemented */
static int fffile_nonblock(fffd fd, int nonblock);
#endif

/** Reposition file offset
method: enum FFFILE_SEEK
Return resulting offset
  <0 on error */
static ffint64 fffile_seek(fffd fd, ffuint64 pos, int method);

/** Advise the kernel to optimize file caching for sequential data access
size: the advice extends from 0 to this offset
  -1: restore system default behaviour
  Note the difference:
    Linux: 0: use the whole file
    FreeBSD, macOS: 0: disable read-ahead
Return !=0 on error
Windows: not supported, but you can use fffile_open(FILE_FLAG_SEQUENTIAL_SCAN)
Android: not supported */
static int fffile_readahead(fffd fd, ffint64 size);

/** Write to a file descriptor
Return N of bytes written
  <0 on error */
static ffssize fffile_write(fffd fd, const void *data, ffsize size);

/** Write at specified offset
Return N of bytes written
  <0 on error */
static ffssize fffile_writeat(fffd fd, const void *data, ffsize size, ffuint64 off);

/** Read from a file descriptor
Return N of bytes read
  <0 on error */
static ffssize fffile_read(fffd fd, void *buf, ffsize size);

/** Read at specified offset
Return N of bytes read
  <0 on error */
static ffssize fffile_readat(fffd fd, void *buf, ffsize size, ffuint64 off);

/** Truncate a file to a specified length
Windows: un-aligned truncate on a file with FFFILE_DIRECT fails with ERROR_INVALID_PARAMETER
 due to un-aligned seeking request. */
static int fffile_trunc(fffd fd, ffuint64 len);


#include <FFOS/file-compat.h>

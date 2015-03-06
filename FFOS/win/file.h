/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/time.h>
#include <FFOS/error.h>
#include <FFOS/win/str.h>

enum {
	FF_MAXPATH = 4096
	, FF_MAXFN = 256
};

enum FFFILE_OPEN {
	// 0x0000 000f.  mode
	O_CREAT = OPEN_ALWAYS
	, FFO_CREATENEW = CREATE_NEW
	, FFO_APPEND = 0x0e //hack

	// 0x0000 00f0.  access
	, O_RDONLY = GENERIC_READ >> 24
	, O_WRONLY = GENERIC_WRITE >> 24
	, O_RDWR = (GENERIC_READ | GENERIC_WRITE) >> 24

	// 0xffff ff00.  flags
	, O_NOATIME = 0
	, O_NONBLOCK = 0
	, O_DIRECT = FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED
	, FFO_NODOSNAME = 0x00000100
	, O_TRUNC = 0x00000200
};

FF_EXTN fffd fffile_openq(const ffsyschar *filename, int flags);
FF_EXTN fffd fffile_open(const char *filename, int flags);

#define fffile_createtempq(filename, flags) \
	fffile_openq(filename, FFO_CREATENEW | FILE_FLAG_DELETE_ON_CLOSE | (flags))

#define fffile_createtemp(filename, flags) \
	fffile_open(filename, FFO_CREATENEW | FILE_FLAG_DELETE_ON_CLOSE | (flags))

static FFINL ssize_t fffile_read(fffd fd, void *buf, size_t size) {
	DWORD read;
	if (0 == ReadFile(fd, buf, FF_TOINT(size), &read, 0))
		return -1;
	return read;
}

static FFINL ssize_t fffile_write(fffd fd, const void *buf, size_t size) {
	DWORD written;
	if (0 == WriteFile(fd, buf, FF_TOINT(size), &written, 0))
		return -1;
	return written;
}

static FFINL int64 fffile_seek(fffd fd, int64 pos, int method) {
	int64 ret;
	if (0 == SetFilePointerEx(fd, *(LARGE_INTEGER *)&pos, (LARGE_INTEGER *)&ret, method))
		return -1;
	return ret;
}

FF_EXTN int fffile_trunc(fffd f, uint64 pos);

#define fffile_readahead(f, size)  0

static FFINL fffd fffile_dup(fffd hdl) {
	fffd ret = INVALID_HANDLE_VALUE;
	HANDLE curProc = GetCurrentProcess();
	DuplicateHandle(curProc, hdl, curProc, &ret, 0, 0, DUPLICATE_SAME_ACCESS);
	return ret;
}

static FFINL int fffile_close(fffd fd) {
	return 0 == CloseHandle(fd);
}

static FFINL int64 fffile_size(fffd fd) {
	uint64 size = 0;
	if (0 == GetFileSizeEx(fd, (LARGE_INTEGER *)&size))
		return -1;
	return size;
}

#define fffile_isdir(file_attr)  (((file_attr) & FILE_ATTRIBUTE_DIRECTORY) != 0)

static FFINL int fffile_attrset(fffd fd, uint new_attr) {
	FILE_BASIC_INFO i;
	memset(&i, 0, sizeof(FILE_BASIC_INFO));
	i.FileAttributes = new_attr;
	return 0 == SetFileInformationByHandle(fd, FileBasicInfo, &i, sizeof(FILE_BASIC_INFO));
}

#define fffile_attrsetfn(fn, attr)  (0 == SetFileAttributes(fn, attr))


typedef BY_HANDLE_FILE_INFORMATION fffileinfo;

static FFINL int fffile_info(fffd fd, fffileinfo *fi) {
	return 0 == GetFileInformationByHandle(fd, fi);
}

/**
Note: fffile_infoid() won't work. */
FF_EXTN int fffile_infofn(const char *fn, fffileinfo *fi);

FF_EXTN fftime _ff_ftToTime(const FILETIME *ft);
FF_EXTN FILETIME _ff_timeToFt(const fftime *time_value);

#define fffile_infomtime(fi)  _ff_ftToTime(&(fi)->ftLastWriteTime)

#define fffile_infosize(fi)  (((uint64)(fi)->nFileSizeHigh << 32) | (fi)->nFileSizeLow)

#define fffile_infoattr(fi)  (fi)->dwFileAttributes

typedef uint64 fffileid;

#define fffile_infoid(fi)  (((uint64)(fi)->nFileIndexHigh << 32) | (fi)->nFileIndexLow)


#define fffile_renameq(src, dst)  (0 == MoveFileEx(src, dst, /*MOVEFILE_COPY_ALLOWED*/ MOVEFILE_REPLACE_EXISTING))

FF_EXTN int fffile_rename(const char *src, const char *dst);

#define fffile_hardlink(target, linkname)  (0 == CreateHardLink(linkname, target, NULL))

static FFINL int fffile_rmq(const ffsyschar *name) {
	return 0 == DeleteFile(name);
}

/**
Note: fails with error "Access is denied" if file mapping is opened. */
FF_EXTN int fffile_rm(const char *name);


#define ffstdin  GetStdHandle(STD_INPUT_HANDLE)
#define ffstdout  GetStdHandle(STD_OUTPUT_HANDLE)
#define ffstderr  GetStdHandle(STD_ERROR_HANDLE)

static FFINL ssize_t ffstd_read(fffd h, ffsyschar * s, size_t len) {
	DWORD r;
	if (0 != ReadConsole(h, s, (uint)len, &r, NULL))
		return r;
	return 0;
}

static FFINL ssize_t ffstd_write(fffd h, const ffsyschar *s, size_t len) {
	DWORD wr;
	if (0 != WriteConsole(h, s, FF_TOINT(len), &wr, NULL))
		return wr;
	return 0;
}


static FFINL int ffpipe_create(fffd *rd, fffd *wr) {
	SECURITY_ATTRIBUTES sa = {
		sizeof(SECURITY_ATTRIBUTES),
		NULL,
		1
	};
	return 0 == CreatePipe(rd, wr, &sa, 0);
}

/// return FF_BADFD on error
static FFINL fffd ffpipe_createnamed(const ffsyschar *name) {
	return CreateNamedPipe(name
		, PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_OVERLAPPED
		, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_ACCEPT_REMOTE_CLIENTS
		, PIPE_UNLIMITED_INSTANCES, 512, 512, 0, NULL);
}

/// Server kicks the client from the pipe
#define ffpipe_disconnect  DisconnectNamedPipe

#define ffpipe_close  fffile_close

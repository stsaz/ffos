/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/time.h>
#include <FFOS/error.h>
#include <FFOS/string.h>

enum {
	FF_MAXPATH = 4096
	, FF_MAXFN = 256
};

enum FFFILE_OPEN {
	//obsolete:
	O_CREAT = OPEN_ALWAYS
	, O_RDONLY = GENERIC_READ >> 24
	, O_WRONLY = GENERIC_WRITE >> 24
	, O_RDWR = (GENERIC_READ | GENERIC_WRITE) >> 24

	// 0x0000 000f.  mode
	, FFO_CREATE = OPEN_ALWAYS
	, FFO_CREATENEW = CREATE_NEW
	, FFO_APPEND = 0x0e //hack

	// 0x0000 00f0.  access
	, FFO_RDONLY = GENERIC_READ >> 24
	, FFO_WRONLY = GENERIC_WRITE >> 24
	, FFO_RDWR = (GENERIC_READ | GENERIC_WRITE) >> 24

	// 0xffff ff00.  flags
	, O_NOATIME = 0
	, O_NONBLOCK = 0
	, O_DIRECT = FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED
	, FFO_NODOSNAME = 0x00000100 //opening a file with 8.3 name will fail
	, O_TRUNC = 0x00000200

	// for ffpipe_create2()
	, FFO_NONBLOCK = 0x00000400
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

#if FF_WIN >= 0x0600
static FFINL int fffile_attrset(fffd fd, uint new_attr) {
	FILE_BASIC_INFO i;
	memset(&i, 0, sizeof(FILE_BASIC_INFO));
	i.FileAttributes = new_attr;
	return 0 == SetFileInformationByHandle(fd, FileBasicInfo, &i, sizeof(FILE_BASIC_INFO));
}
#endif

#define fffile_attrsetfnq(fn, attr)  (0 == SetFileAttributes(fn, attr))

FF_EXTN int fffile_attrsetfn(const char *fn, uint attr);


#define fffile_chown(fd, uid, gid)  (-1)


typedef BY_HANDLE_FILE_INFORMATION fffileinfo;

static FFINL int fffile_info(fffd fd, fffileinfo *fi) {
	return 0 == GetFileInformationByHandle(fd, fi);
}

/**
Note: fffile_infoid() won't work. */
FF_EXTN int fffile_infofn(const char *fn, fffileinfo *fi);

#define fffile_infomtime(fi)  fftime_from_winftime((void*)&(fi)->ftLastWriteTime)

#define fffile_infosize(fi)  (((uint64)(fi)->nFileSizeHigh << 32) | (fi)->nFileSizeLow)

#define fffile_infoattr(fi)  (fi)->dwFileAttributes

typedef uint64 fffileid;

#define fffile_infoid(fi)  (((uint64)(fi)->nFileIndexHigh << 32) | (fi)->nFileIndexLow)

static FFINL int fffile_settime(fffd fd, const fftime *last_write)
{
	FILETIME ft;
	fftime_to_winftime(last_write, (fftime_winftime*)&ft);
	return 0 == SetFileTime(fd, NULL, NULL, &ft);
}

static FFINL int fffile_settimefn(const char *fn, const fftime *last_write)
{
	fffd f;
	if (FF_BADFD == (f = fffile_open(fn, O_WRONLY)))
		return -1;
	int r = fffile_settime(f, last_write);
	fffile_close(f);
	return r;
}


#define fffile_renameq(src, dst)  (0 == MoveFileEx(src, dst, /*MOVEFILE_COPY_ALLOWED*/ MOVEFILE_REPLACE_EXISTING))

FF_EXTN int fffile_rename(const char *src, const char *dst);

#define fffile_hardlinkq(target, linkname)  (0 == CreateHardLink(linkname, target, NULL))

FF_EXTN int fffile_hardlink(const char *target, const char *linkname);

static FFINL int fffile_symlink(const char *target, const char *linkname)
{
	(void)target; (void)linkname;
	fferr_set(ENOSYS);
	return -1;
}

FF_EXTN int fffile_rmq(const ffsyschar *name);

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

#define ffstd_fread(fd, d, len)  ffpipe_read(fd, d, len)

static FFINL ssize_t ffstd_writeq(fffd h, const ffsyschar *s, size_t len) {
	DWORD wr;
	if (0 != WriteConsole(h, s, FF_TOINT(len), &wr, NULL))
		return wr;
	return 0;
}

/** Write UTF-8 data into standard output/error handle. */
FF_EXTN ssize_t ffstd_write(fffd h, const char *s, size_t len);

typedef struct ffstd_ev {
	uint state;
	INPUT_RECORD rec[8];
	uint irec;
	uint nrec;

	char *data;
	size_t datalen;
} ffstd_ev;

FF_EXTN int ffstd_event(fffd fd, ffstd_ev *ev);

#define ffterm_detach()  FreeConsole()


// UNNAMED/NAMED PIPES

static FFINL int ffpipe_create(fffd *rd, fffd *wr) {
	return 0 == CreatePipe(rd, wr, NULL, 0);
}

FF_EXTN int ffpipe_create2(fffd *rd, fffd *wr, uint flags);

static inline int ffpipe_nblock(fffd p, int nblock)
{
	DWORD mode = PIPE_READMODE_BYTE;
	mode |= (nblock) ? PIPE_NOWAIT : 0;
	return !SetNamedPipeHandleState(p, &mode, NULL, NULL);
}

/// return FF_BADFD on error
static FFINL fffd ffpipe_create_namedq(const ffsyschar *name, uint flags)
{
	(void)flags;
	return CreateNamedPipe(name
		, PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_OVERLAPPED
		, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT
		, PIPE_UNLIMITED_INSTANCES, 512, 512, 0, NULL);
}

FF_EXTN fffd ffpipe_create_named(const char *name, uint flags);

#define ffpipe_peer_close(fd)  DisconnectNamedPipe(fd)

#define ffpipe_close  fffile_close

#define ffpipe_connect(name) fffile_open(name, O_RDWR)

#define ffpipe_client_close(fd)  fffile_close(fd)

FF_EXTN ssize_t ffpipe_read(fffd fd, void *buf, size_t cap);

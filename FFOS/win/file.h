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

enum {
	// for ffpipe_create2()
	FFO_NONBLOCK = 0x00000400
};

typedef uint64 fffileid;

/** Create or delete a mount point for disk.
When deleting, the mount point directory isn't deleted.
disk: volume GUI name: "\\?\Volume{GUID}\"
 NULL: delete mount point
mount: mount point (an existing empty directory) */
FF_EXTN int fffile_mount(const char *disk, const char *mount);


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

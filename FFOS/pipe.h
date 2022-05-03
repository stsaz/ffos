/** ffos: named/unnamed pipe
2020, Simon Zolin
*/

/*
ffpipe_create ffpipe_create2 ffpipe_create_named
ffpipe_connect
ffpipe_accept ffpipe_accept_async
ffpipe_close ffpipe_peer_close
ffpipe_nonblock
ffpipe_read
ffpipe_write
*/

#pragma once

#include <FFOS/base.h>
#include <FFOS/kqtask.h>

#define FFPIPE_ASYNC  2

#ifdef FF_WIN

#include <FFOS/string.h>

#define FFPIPE_NULL  INVALID_HANDLE_VALUE
#define FFPIPE_NONBLOCK  1
#define FFPIPE_NAME_PREFIX  "\\\\.\\pipe\\"
#define FFPIPE_EINPROGRESS  ERROR_IO_PENDING

static inline int ffpipe_create(fffd *rd, fffd *wr)
{
	return !CreatePipe(rd, wr, NULL, 0);
}

static inline int ffpipe_nonblock(fffd p, int nonblock)
{
	DWORD mode = PIPE_READMODE_BYTE;
	mode |= (nonblock) ? PIPE_NOWAIT : 0;
	return !SetNamedPipeHandleState(p, &mode, NULL, NULL);
}

static inline int ffpipe_create2(fffd *rd, fffd *wr, ffuint flags)
{
	if (0 != ffpipe_create(rd, wr))
		return -1;

	if (flags & FFPIPE_NONBLOCK) {
		if (0 != ffpipe_nonblock(*rd, 1)
			|| 0 != ffpipe_nonblock(*wr, 1)) {
			CloseHandle(*rd);
			CloseHandle(*wr);
			return -1;
		}
	}

	return 0;
}

static inline fffd ffpipe_create_named(const char *name, ffuint flags)
{
	if (flags & FFPIPE_NONBLOCK) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return FFPIPE_NULL;
	}

	wchar_t *w, ws[256];
	if (NULL == (w = ffsz_alloc_buf_utow(ws, FF_COUNT(ws), name)))
		return FFPIPE_NULL;

	ffuint f = (flags & FFPIPE_ASYNC) ? FILE_FLAG_OVERLAPPED : 0;
	fffd p = CreateNamedPipeW(w
		, PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE | f
		, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT
		, PIPE_UNLIMITED_INSTANCES, 512, 512, 0, NULL);
	if (w != ws)
		ffmem_free(w);
	return p;
}

static inline void ffpipe_peer_close(fffd p)
{
	DisconnectNamedPipe(p);
}

static inline void ffpipe_close(fffd p)
{
	CloseHandle(p);
}

static inline fffd ffpipe_accept(fffd listen_pipe)
{
	BOOL b = ConnectNamedPipe(listen_pipe, NULL);
	if (!b) {
		if (GetLastError() == ERROR_PIPE_CONNECTED)
			return listen_pipe; // the client has connected before we called ConnectNamedPipe()
		return FFPIPE_NULL;
	}
	return listen_pipe;
}

static inline fffd ffpipe_accept_async(fffd listen_pipe, ffkq_task *task)
{
	if (task->active) {
		DWORD n;
		if (!GetOverlappedResult(NULL, &task->overlapped, &n, 0)) {
			if (GetLastError() == ERROR_IO_INCOMPLETE)
				SetLastError(ERROR_IO_PENDING);
			else
				task->active = 0;
			return FFPIPE_NULL;
		}
		task->active = 0;
		return listen_pipe;
	}

	ffmem_zero_obj(task);
	int ok = 1;
	if (!ConnectNamedPipe(listen_pipe, &task->overlapped)) {
		int e = GetLastError();
		if (e == ERROR_PIPE_CONNECTED)
			return listen_pipe;
		else if (e != ERROR_IO_PENDING)
			ok = 0;
	}
	task->active = ok;
	return FFPIPE_NULL;
}

static inline fffd ffpipe_connect(const char *name)
{
	wchar_t *w, ws[256];
	if (NULL == (w = ffsz_alloc_buf_utow(ws, FF_COUNT(ws), name)))
		return FFPIPE_NULL;

	fffd p = CreateFileW(w, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

	if (w != ws)
		ffmem_free(w);
	return p;
}

static inline ffssize ffpipe_read(fffd p, void *buf, ffsize cap)
{
	DWORD rd;
	if (!ReadFile(p, buf, ffmin(cap, 0xffffffff), &rd, 0)) {
		int e = GetLastError();
		if (e == ERROR_BROKEN_PIPE)
			return 0;
		else if (e == ERROR_NO_DATA)
			SetLastError(WSAEWOULDBLOCK);
		return -1;
	}
	return rd;
}

static inline ffssize ffpipe_write(fffd p, const void *data, ffsize size)
{
	DWORD wr;
	if (!WriteFile(p, data, ffmin(size, 0xffffffff), &wr, 0))
		return -1;
	return wr;
}

#else // UNIX:

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <sys/socket.h>

#define FFPIPE_NULL  (-1)
#define FFPIPE_NONBLOCK  O_NONBLOCK
#define FFPIPE_NAME_PREFIX  ""
#define FFPIPE_EINPROGRESS  EINPROGRESS

static inline int ffpipe_nonblock(fffd p, int nonblock)
{
	return ioctl(p, FIONBIO, &nonblock);
}

static inline int ffpipe_create2(fffd *rd, fffd *wr, ffuint flags)
{
#ifdef FF_APPLE

	fffd p[2];
	if (0 != pipe(p))
		return -1;
	*rd = p[0];
	*wr = p[1];

	if (flags & FFPIPE_NONBLOCK) {
		if (0 != ffpipe_nonblock(*rd, 1)
			|| 0 != ffpipe_nonblock(*wr, 1)) {
			close(*rd);
			close(*wr);
			return -1;
		}
	}

	return 0;

#else // not macOS:

	fffd p[2];
	if (0 != pipe2(p, flags))
		return -1;
	*rd = p[0];
	*wr = p[1];
	return 0;
#endif
}

static inline int ffpipe_create(fffd *rd, fffd *wr)
{
	return ffpipe_create2(rd, wr, 0);
}

static inline fffd ffpipe_create_named(const char *name, ffuint flags)
{
	fffd p;
	struct sockaddr_un a = {};
#if defined FF_BSD || defined FF_APPLE
	a.sun_len = sizeof(struct sockaddr_un);
#endif
	a.sun_family = AF_UNIX;
	ffsize len = ffsz_len(name);
	if (len + 1 > sizeof(a.sun_path))
		return FFPIPE_NULL;
	strcpy(a.sun_path, name);

	if (FFPIPE_NULL == (p = socket(AF_UNIX, SOCK_STREAM, 0)))
		return FFPIPE_NULL;

	if (flags & (FFPIPE_NONBLOCK | FFPIPE_ASYNC)) {
		if (0 != ffpipe_nonblock(p, 1))
			goto err;
	}

	if (0 != bind(p, (struct sockaddr*)&a, sizeof(struct sockaddr_un)))
		goto err;

	if (0 != listen(p, 0))
		goto err;

	return p;

err:
	close(p);
	return FFPIPE_NULL;
}

static inline void ffpipe_close(fffd p)
{
	close(p);
}

static inline void ffpipe_peer_close(fffd p)
{
	close(p);
}

static inline fffd ffpipe_connect(const char *name)
{
	fffd p;
	struct sockaddr_un a = {};
#if defined FF_BSD || defined FF_APPLE
	a.sun_len = sizeof(struct sockaddr_un);
#endif
	a.sun_family = AF_UNIX;
	ffsize len = ffsz_len(name);
	if (len + 1 > sizeof(a.sun_path)) {
		errno = EINVAL;
		return FFPIPE_NULL;
	}
	ffmem_copy(a.sun_path, name, len + 1);

	if (-1 == (p = socket(AF_UNIX, SOCK_STREAM, 0)))
		return FFPIPE_NULL;

	if (0 != connect(p, (struct sockaddr*)&a, sizeof(struct sockaddr_un))) {
		close(p);
		return FFPIPE_NULL;
	}

	return p;
}

static inline fffd ffpipe_accept(fffd listen_pipe)
{
	return accept(listen_pipe, NULL, NULL);
}

static inline fffd ffpipe_accept_async(fffd listen_pipe, ffkq_task *task)
{
	(void)task;
	int c;
	if (-1 == (c = accept(listen_pipe, NULL, NULL))) {
		if (errno == EAGAIN)
			errno = EINPROGRESS;
		return -1;
	}
	return c;
}

static inline ffssize ffpipe_read(fffd p, void *buf, ffsize size)
{
	return read(p, buf, size);
}

static inline ffssize ffpipe_write(fffd p, const void *buf, ffsize size)
{
	return write(p, buf, size);
}

#endif


/** Create an unnamed pipe
flags: FFPIPE_NONBLOCK
Return !=0 on error */
static int ffpipe_create2(fffd *rd, fffd *wr, ffuint flags);

static int ffpipe_create(fffd *rd, fffd *wr);

/** Create named pipe
name:
  UNIX: file name to be used for UNIX socket
  Windows: \\.\pipe\NAME
flags:
  UNIX: FFPIPE_NONBLOCK | FFPIPE_ASYNC
  Windows: FFPIPE_ASYNC
Return FFPIPE_NULL on error */
static fffd ffpipe_create_named(const char *name, ffuint flags);

/** Close a pipe */
static void ffpipe_close(fffd p);

/** Close a peer pipe descriptor returned by ffpipe_accept() */
static void ffpipe_peer_close(fffd p);

/** Connect to a named pipe
Return FFPIPE_NULL on error */
static fffd ffpipe_connect(const char *name);

/** Accept an inbound connection to a named pipe
Windows: non-blocking mode is not supported.
Return pipe descriptor; close with ffpipe_peer_close()
    Windows: the same pipe fd is returned
    UNIX: new fd to a UNIX socket's peer is returned
  FFPIPE_NULL on error */
static fffd ffpipe_accept(fffd listen_pipe);

/** Same as ffpipe_accept(), except in case it can't complete immediately,
 it begins asynchronous operation and returns FFPIPE_NULL with error FFPIPE_EINPROGRESS.
Pipe must be opened with ffpipe_create_named(FFPIPE_ASYNC). */
static fffd ffpipe_accept_async(fffd listen_pipe, ffkq_task *task);

/** Set non-blocking mode on a pipe descriptor */
static int ffpipe_nonblock(fffd p, int nonblock);

/** Read from a pipe descriptor */
static ffssize ffpipe_read(fffd p, void *buf, ffsize size);

/** Write to a pipe descriptor */
static ffssize ffpipe_write(fffd p, const void *buf, ffsize size);


#define ffpipe_nblock  ffpipe_nonblock
#define ffpipe_client_close  ffpipe_close

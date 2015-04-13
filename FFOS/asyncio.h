/** Asynchronous IO.
Copyright (c) 2013 Simon Zolin
*/

#pragma once

#include <FFOS/queue.h>
#include <FFOS/mem.h>
#include <FFOS/socket.h>


typedef void (*ffaio_handler)(void *udata);

typedef struct ffaio_task {
	ffaio_handler rhandler;
	void *udata;
	union {
		fffd fd;
		ffskt sk;
	};
	unsigned instance :1
		, oneshot :1
		, aiotask :1;

	ffaio_handler whandler;
	int evflags;

#if defined FF_BSD
	ffkqu_entry *ev;

#elif defined FF_WIN
	unsigned canceled :1
		, rsig :1;
	int result;
	OVERLAPPED rovl;
	OVERLAPPED wovl;
#endif
} ffaio_task;

/** Initialize AIO task. */
static FFINL void ffaio_init(ffaio_task *t) {
	unsigned inst = t->instance;
	ffmem_tzero(t);
	t->fd = FF_BADFD;
	t->instance = inst;
	t->oneshot = 1;
	t->aiotask = 1;
}

/** Finalize AIO task. */
static FFINL void ffaio_fin(ffaio_task *t) {
	unsigned inst = t->instance;
	t->fd = FF_BADFD;
	t->udata = NULL;
	t->rhandler = t->whandler = NULL;
	t->instance = !inst;
}

/** Get udata to register in the kernel queue. */
static FFINL void * ffaio_kqudata(ffaio_task *t) {
	return (void*)((size_t)t | t->instance);
}

/** Attach AIO task to kernel queue. */
#define ffaio_attach(task, kq, ev) \
	ffkqu_attach(kq, (task)->fd, ffaio_kqudata(task), FFKQU_ADD | (ev))

enum FFAIO_RET {
	FFAIO_OK = 0
	, FFAIO_ERROR = -1
	, FFAIO_ASYNC = -2
};

typedef struct ffaio_filetask ffaio_filetask;

#ifdef FF_UNIX
#include <FFOS/unix/aio.h>

#else
#include <FFOS/win/aio.h>
#endif

/** Accept client connection.
Return FF_BADSKT on error. */
FF_EXTN ffskt ffaio_accept(ffaio_acceptor *acc, ffaddr *local, ffaddr *peer, int flags, ffkev_handler handler);

/** Call connect on a socket.
Return enum FFAIO_RET. */
FF_EXTN int ffaio_connect(ffaio_task *t, ffaio_handler handler, const struct sockaddr *addr, socklen_t addr_size);


/** Initialize file async I/O task. */
static FFINL void ffaio_finit(ffaio_filetask *ft, fffd fd, void *udata)
{
	ffkev_init(&ft->kev);
	ft->kev.fd = fd;
	ft->kev.udata = udata;
}

/** Asynchronous file I/O.
Return the number of bytes transferred or -1 on error.
  UNIX: Fail with ENOSYS if AIO is not supported by kernel.
Note: cancelling is not supported.
FreeBSD: kernel AIO must be enabled ("kldload aio").
Linux, Windows: file must be opened with O_DIRECT.
Linux: operations on xfs and ext4 may block (3.17).
Windows: writing into a new file on NTFS is always blocking. */
FF_EXTN ssize_t ffaio_fread(ffaio_filetask *ft, void *data, size_t len, uint64 off, ffaio_handler handler);
FF_EXTN ssize_t ffaio_fwrite(ffaio_filetask *ft, const void *data, size_t len, uint64 off, ffaio_handler handler);


/** Return TRUE if AIO task is active. */
#define ffaio_active(a)  ((a)->rhandler != NULL || (a)->whandler != NULL)

enum FFAIO_CANCEL {
	FFAIO_READ = 1
	, FFAIO_WRITE = 2
	, FFAIO_CONNECT = FFAIO_WRITE
	, FFAIO_RW = FFAIO_READ | FFAIO_WRITE
};

/** Cancel AIO task.  Handler function will be called.
@op: enum FFAIO_CANCEL.
@oncancel: if not NULL, set new handler. */
FF_EXTN int ffaio_cancelasync(ffaio_task *t, int op, ffaio_handler oncancel);

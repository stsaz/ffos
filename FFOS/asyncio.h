/** Asynchronous IO.
Copyright (c) 2013 Simon Zolin
*/

#pragma once

#include <FFOS/queue.h>
#include <FFOS/mem.h>
#include <FFOS/socket.h>


typedef void (*ffaio_handler)(void *udata);

/** The extended version of ffkevent: allows full duplex I/O and cancellation. */
typedef struct ffaio_task {
	ffaio_handler rhandler;
	void *udata;
	union {
		fffd fd;
		ffskt sk;
	};
	unsigned instance :1
		, oneshot :1
		, aiotask :1
		, rpending :1
		, wpending :1
		, udp :1 // Windows: set for UDP socket
		, canceled :1
		;

	ffaio_handler whandler;
	ffkqu_entry *ev;

#if defined FF_WIN
	byte sendbuf[1]; //allows transient buffer to be used for ffaio_send()
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
	FFAIO_ERROR = -1
	, FFAIO_ASYNC = -2
};

typedef struct ffaio_filetask ffaio_filetask;

#ifdef FF_UNIX
#include <FFOS/unix/aio.h>

#else
#include <FFOS/win/aio.h>
#endif

/** Async socket receive.
Windows: async operation is scheduled with an empty buffer.
Return bytes received or enum FFAIO_RET. */
FF_EXTN int ffaio_recv(ffaio_task *t, ffaio_handler handler, void *d, size_t cap);

/** Async socket send.
Windows: no more than 1 byte is sent using async operation.
Return bytes sent or enum FFAIO_RET. */
FF_EXTN int ffaio_send(ffaio_task *t, ffaio_handler handler, const void *d, size_t len);

FF_EXTN int ffaio_sendv(ffaio_task *t, ffaio_handler handler, ffiovec *iov, size_t iovcnt);

/** Accept client connection.
Return FF_BADSKT on error. */
FF_EXTN ffskt ffaio_accept(ffaio_acceptor *acc, ffaddr *local, ffaddr *peer, int flags, ffkev_handler handler);

/** Connect to a remote server.
Return 0 on success or enum FFAIO_RET. */
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
Note: cancelling is not supported.
FreeBSD: kernel AIO must be enabled ("kldload aio"), or operations will block.
Linux, Windows: file must be opened with O_DIRECT, or operations will block.
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

#if !defined FF_WIN || FF_WIN >= 0x0600
/** Cancel AIO task.  Handler function will be called.
@op: enum FFAIO_CANCEL.
@oncancel: if not NULL, set new handler. */
FF_EXTN int ffaio_cancelasync(ffaio_task *t, int op, ffaio_handler oncancel);
#endif

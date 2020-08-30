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

#define fffile_createtempq  fffile_createtemp

typedef ino_t fffileid;

#define ffterm_detach()


// UNNAMED/NAMED PIPES

/** Create pipe.
flags: FFO_NONBLOCK */
FF_EXTN int ffpipe_create2(fffd *rd, fffd *wr, uint flags);

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

/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/file.h>

#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>

#if defined FF_BSD
#include <sys/uio.h>
#endif


enum {
	FF_BADSKT = -1
	, FF_MAXIP4 = 22
	, FF_MAXIP6 = 65 // "[addr]:port"

#ifdef FF_BSD
	, SOCK_NONBLOCK = 0
#endif
};

#ifdef FF_LINUX
#define TCP_NOPUSH  TCP_CORK
#endif

/** Prepare using sockets.
flags: enum FFSKT_INIT. */
static FFINL int ffskt_init(int flags) {
	signal(SIGPIPE, SIG_IGN);
	return 0;
}

#ifdef FF_LINUX
#if !defined FF_OLDLIBC
/** Accept a connection on a socket.
flags: SOCK_NONBLOCK
Does not inherit non-blocking mode. */
static FFINL ffskt ffskt_accept(ffskt listenSk, struct sockaddr *a, socklen_t *addrlen, int flags) {
	return accept4(listenSk, a, addrlen, flags);
}

#else
FF_EXTN ffskt ffskt_accept(ffskt listenSk, struct sockaddr *a, socklen_t *addrlen, int flags);
#endif

#define ffskt_deferaccept(sk, enable) \
	ffskt_setopt(sk, IPPROTO_TCP, TCP_DEFER_ACCEPT, enable)

#elif defined FF_BSD
/** Inherits non-blocking mode. */
#define ffskt_accept(listenSk, a, addrlen, flags)\
	accept(listenSk, a, addrlen)

static FFINL int ffskt_deferaccept(ffskt sk, int enable)
{
	struct accept_filter_arg af;
	if (enable) {
		memset(&af, 0, sizeof(struct accept_filter_arg));
		strcpy(af.af_name, "dataready");
	}
	return setsockopt(sk, SOL_SOCKET, SO_ACCEPTFILTER, (enable ? &af : NULL), sizeof(struct accept_filter_arg));
}

#endif

/** Receive data from a socket.
Return -1 on error. */
static FFINL ssize_t ffskt_recv(ffskt fd, void *buf, size_t size, int flags) {
	return recv(fd, (char*)buf, size, flags);
}

/** Send data to a socket.
Return -1 on error. */
static FFINL ssize_t ffskt_send(ffskt fd, const void *buf, size_t size, int flags) {
	return send(fd, (char*)buf, size, flags);
}


typedef struct iovec ffiovec;

/** Send data from multiple buffers. */
#define ffskt_sendv  writev

#ifdef FF_LINUX

/** Headers and trailers for sendfile() function. */
typedef struct sf_hdtr {
	ffiovec *headers;
	int hdr_cnt;
	ffiovec *trailers;
	int trl_cnt;
} sf_hdtr;

/** Send a file to a socket.
Return 0 on success. */
FF_EXTN int ffskt_sendfile(ffskt sk, fffd fd, uint64 offs, uint64 sz, sf_hdtr *hdtr, uint64 *sent, int flags);

#elif defined FF_BSD

typedef struct sf_hdtr sf_hdtr;

/* if sz == 0, send until eof */
static FFINL int ffskt_sendfile(ffskt sk, fffd fd, uint64 offs, uint64 sz, sf_hdtr *hdtr, uint64 *_sent, int flags) {
	off_t sent = 0;
	int r = sendfile(fd, sk, /*(off_t)*/offs, FF_TOSIZE(sz), hdtr, &sent, flags);
	*_sent = sent;
	return r;
}

#endif


/** Send TCP FIN. */
#define ffskt_fin(sk)  shutdown(sk, SHUT_WR)

/** Close a socket. */
#define ffskt_close  close

/** Set non-blocking mode on a socket. */
#define ffskt_nblock  fffile_nblock


typedef struct addrinfo ffaddrinfo;

/** Translate name to network address.
Return 0 on success.
Example:
	ffaddrinfo *addrs;
	ffaddr_info(&addrs, TEXT("localhost"), TEXT("80"), AI_PASSIVE); */
static FFINL int ffaddr_info(ffaddrinfo **a, const ffsyschar *host, const ffsyschar *svc, int flags)
{
	ffaddrinfo hints = { 0 };
	hints.ai_flags = flags;
	return getaddrinfo(host, svc, &hints, a);
}

/** Free ffaddrinfo structure. */
#define ffaddr_free freeaddrinfo

/** Address-to-name translation.
Max host length: NI_MAXHOST
Max service length: NI_MAXSERV
flags: NI_* */
#define ffaddr_name(a, addrlen, host, hostcap, svc, svccap, flags)\
	getnameinfo(a, addrlen, host, (socklen_t)(hostcap), svc, (socklen_t)(svccap), flags)

/** Get error message. */
#define ffaddr_errstr(code, buf, bufcap)  gai_strerror(code)

/** Return TRUE if both IPv6 addresses are equal. */
#define ffip6_eq  IN6_ARE_ADDR_EQUAL

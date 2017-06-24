/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/file.h>

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
};

/** Prepare using sockets.
flags: enum FFSKT_INIT. */
static FFINL int ffskt_init(int flags) {
	if (flags & FFSKT_SIGPIPE)
		signal(SIGPIPE, SIG_IGN);
	return 0;
}

/** Create an endpoint for communication.
@type: SOCK_*
Return FF_BADSKT on error.
Example:
ffskt sk = ffskt_create(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP); */
#define ffskt_create(domain, type, protocol) \
	socket(domain, type, protocol)

#ifndef FF_OLDLIBC
/** Accept a connection on a socket.
flags: SOCK_NONBLOCK. */
static FFINL ffskt ffskt_accept(ffskt listenSk, struct sockaddr *a, socklen_t *addrlen, int flags) {
	return accept4(listenSk, a, addrlen, flags);
}
#else
FF_EXTN ffskt ffskt_accept(ffskt listenSk, struct sockaddr *a, socklen_t *addrlen, int flags);
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
#define ffskt_sendv(sk, iov, iovcnt)  writev(sk, iov, iovcnt)

typedef struct sf_hdtr sf_hdtr;

/** Send a file to a socket.
@hdtr: optional.
Return 0 on success. */
FF_EXTN int ffskt_sendfile(ffskt sk, fffd fd, uint64 offs, uint64 sz, sf_hdtr *hdtr, uint64 *sent, int flags);


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
	ffaddr_info(&addrs, "localhost", "80", AI_PASSIVE); */
static FFINL int ffaddr_info(ffaddrinfo **a, const char *host, const char *svc, int flags)
{
	ffaddrinfo hints = { 0 };
	hints.ai_socktype = SOCK_DGRAM; //filter out results with the same IP but different ai_socktype
	hints.ai_flags = flags;
	return getaddrinfo(host, svc, &hints, a);
}

#define ffaddr_infoq ffaddr_info

/** Free ffaddrinfo structure. */
#define ffaddr_free freeaddrinfo

/** Address-to-name translation.
Max host length: NI_MAXHOST
Max service length: NI_MAXSERV
flags: NI_* */
#define ffaddr_name(a, addrlen, host, hostcap, svc, svccap, flags)\
	getnameinfo(a, addrlen, host, (socklen_t)(hostcap), svc, (socklen_t)(svccap), flags)

/** Get error message. */
static FFINL const char * ffaddr_errstr(int code, char *buf, size_t bufcap) {
	return gai_strerror(code);
}

/** Return TRUE if both IPv6 addresses are equal. */
#define ffip6_eq  IN6_ARE_ADDR_EQUAL

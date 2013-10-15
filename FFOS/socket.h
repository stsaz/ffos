/**
Socket, network address, IPv4/IPv6 address functions.
Copyright (c) 2013 Simon Zolin
*/

#pragma once

#include <FFOS/types.h>

#include <string.h>


#ifdef FF_UNIX
typedef int ffskt;
typedef void * ffsktopt_t;

#elif defined FF_WIN
typedef SOCKET ffskt;
typedef char * ffsktopt_t;
#endif

enum FFSKT_INIT {
	FFSKT_WSA = 1
	, FFSKT_WSAFUNCS = 2
};


#ifdef FF_UNIX
#include <FFOS/unix/skt.h>
#elif defined FF_WIN
#include <FFOS/win/skt.h>
#endif


typedef struct {
	socklen_t len;
	union {
		struct sockaddr a;
		struct sockaddr_in ip4;
		struct sockaddr_in6 ip6;
	};
} ffaddr;

/** Initialize ffaddr structure. */
static FFINL void ffaddr_init(ffaddr *adr, int family, socklen_t len) {
	memset(adr, 0, sizeof(ffaddr));
	adr->a.sa_family = family;
	adr->len = len;
}

/** Get address family. */
#define ffaddr_family(adr)  ((adr)->a.sa_family)

/** Set port (host byte order). */
static FFINL void ffip_setport(ffaddr *adr, uint port) {
	port = ffhton16((ushort)port);
	if (adr->a.sa_family == AF_INET)
		adr->ip4.sin_port = (short)port;
	else //if (adr->a.sa_family == AF_INET6)
		adr->ip6.sin6_port = (short)port;
}

/** Get port (host byte order). */
static FFINL ushort ffip_port(ffaddr *adr) {
	return adr->a.sa_family == AF_INET
		? ffhton16(adr->ip4.sin_port)
		: ffhton16(adr->ip6.sin6_port);
}

/** Set IPv4 address (host byte order).
Example:
ffip_setv4addr(&ip4, INADDR_LOOPBACK); */
static FFINL void ffip_setv4addr(struct sockaddr_in *ip4, int net_addr) {
	ip4->sin_addr.s_addr = ffhton32(net_addr);
}

/** Set IPv4-mapped address (host byte order). */
static FFINL void ffip_setv4mapped(struct sockaddr_in6 *ip6, int net_addr) {
	int *a = (int*)&ip6->sin6_addr;
	a[0] = a[1] = 0;
	a[2] = ffhton32(0x0000ffff);
	a[3] = ffhton32(net_addr);
}

/** Set IPv6 address.
Example:
ffip_setv6addr(&ip6, &in6addr_loopback); */
static FFINL void ffip_setv6addr(struct sockaddr_in6 *ip6, const struct in6_addr *net_addr) {
	ip6->sin6_addr = *net_addr;
}

/** Return TRUE for IPv4-mapped address. */
#define ffip_v4mapped  IN6_IS_ADDR_V4MAPPED

/** Convert IPv4-mapped address to IPv4. */
static FFINL void ffip_v4mapped_tov4(ffaddr *a) {
	a->len = sizeof(struct sockaddr_in);
	a->a.sa_family = AF_INET;
	a->ip4.sin_port = a->ip6.sin6_port;
	a->ip4.sin_addr.s_addr = *((int *)(&a->ip6.sin6_addr) + 3);
}


/** Create an endpoint for communication.
Return FF_BADSKT on error.
Example:
ffskt sk = ffskt_create(AF_INET, SOCK_STREAM, IPPROTO_TCP); */
#define ffskt_create  socket

/** Initiate a connection on a socket. */
static FFINL int ffskt_connect(ffskt sk, const struct sockaddr *addr, size_t addrlen) {
	return connect(sk, addr, (socklen_t)addrlen);
}

/** Listen for connections on a socket.
Example:
ffskt_listen(lsn_sk, SOMAXCONN); */
#define ffskt_listen  listen

/** Set option on a socket.
Example:
ffskt_setopt(sk, IPPROTO_TCP, TCP_NODELAY, 1); */
static FFINL int ffskt_setopt(ffskt sk, int lev, int optname, int val) {
	return setsockopt(sk, lev, optname, (ffsktopt_t)&val, sizeof(int));
}

/** Get socket option. */
static FFINL int ffskt_getopt(ffskt h, int lev, int opt, int *dst) {
	socklen_t len = sizeof(int);
	return getsockopt(h, lev, opt, (ffsktopt_t)dst, &len);
}

/** Bind a name to a socket. */
static FFINL int ffskt_bind(ffskt sk, const struct sockaddr *addr, size_t addrlen) {
	(void)ffskt_setopt(sk, SOL_SOCKET, SO_REUSEADDR, 1);
	return bind(sk, addr, (socklen_t)addrlen);
}

/** Get local socket address.
Return 0 on success. */
static FFINL int ffskt_local(ffskt sk, ffaddr *adr) {
	socklen_t size = sizeof(struct sockaddr_in6);
	if (0 != getsockname(sk, &adr->a, &size)
		|| size > sizeof(struct sockaddr_in6))
		return -1;
	adr->len = size;
	return 0;
}


/** Set buffer in ffiovec structure. */
static FFINL void ffiov_set(ffiovec *iov, const void *data, size_t len) {
	iov->iov_base = (char*)data;
#if defined FF_UNIX
	iov->iov_len = len;
#elif defined FF_WIN
	iov->iov_len = FF_TOINT(len);
#endif
}

/** Shift buffer position in ffiovec structure. */
static FFINL void ffiov_shift(ffiovec *iov, size_t len) {
	ffiov_set(iov, (char*)iov->iov_base + len, (size_t)iov->iov_len - len);
}

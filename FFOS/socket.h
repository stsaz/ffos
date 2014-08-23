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


#ifdef FF_LINUX
#include <FFOS/unix/skt.h>
#include <FFOS/unix/skt-linux.h>

#elif defined FF_BSD
#include <FFOS/unix/skt.h>
#include <FFOS/unix/skt-bsd.h>

#elif defined FF_WIN
#include <FFOS/win/skt.h>
#endif


typedef struct ffaddr {
	socklen_t len;
	union {
		struct sockaddr a;
		struct sockaddr_in ip4;
		struct sockaddr_in6 ip6;
	};
} ffaddr;

enum {
	FFADDR_MAXLEN = sizeof(struct sockaddr_in6)
};

/** Initialize ffaddr structure. */
static FFINL void ffaddr_init(ffaddr *adr) {
	memset(adr, 0, sizeof(ffaddr));
}

/** Copy address. */
static FFINL void ffaddr_copy(ffaddr *dst, const struct sockaddr *src, size_t len) {
	if (len > FFADDR_MAXLEN) {
		dst->len = 0;
		return;
	}
	memcpy(&dst->a, src, len);
	dst->len = (socklen_t)len;
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
static FFINL ushort ffip_port(const ffaddr *adr) {
	return adr->a.sa_family == AF_INET
		? ffhton16(adr->ip4.sin_port)
		: ffhton16(adr->ip6.sin6_port);
}

/** Set IPv4 address. */
static FFINL void ffip4_set(ffaddr *adr, const struct in_addr *net_addr) {
	adr->ip4.sin_addr = *net_addr;
	adr->a.sa_family = AF_INET;
	adr->len = sizeof(struct sockaddr_in);
}

/** Set IPv4 address (host byte order).
Example:
ffip4_setint(&ip4, INADDR_LOOPBACK); */
static FFINL void ffip4_setint(ffaddr *adr, int net_addr) {
	net_addr = ffhton32(net_addr);
	ffip4_set(adr, (struct in_addr*)&net_addr);
}

/** Set IPv4-mapped address (host byte order). */
static FFINL void ffip6_setv4mapped(ffaddr *adr, int net_addr) {
	int *a = (int*)&adr->ip6.sin6_addr;
	a[0] = a[1] = 0;
	a[2] = ffhton32(0x0000ffff);
	a[3] = ffhton32(net_addr);
	adr->a.sa_family = AF_INET6;
	adr->len = sizeof(struct sockaddr_in6);
}

/** Set IPv6 address.
Example:
ffip6_set(&ip6, &in6addr_loopback); */
static FFINL void ffip6_set(ffaddr *adr, const struct in6_addr *net_addr) {
	adr->ip6.sin6_addr = *net_addr;
	adr->a.sa_family = AF_INET6;
	adr->len = sizeof(struct sockaddr_in6);
}

/** Set "any" address. */
static FFINL void ffaddr_setany(ffaddr *adr, int family) {
	if (family == AF_INET)
		ffip4_setint(adr, INADDR_ANY);
	else
		ffip6_set(adr, &in6addr_any);
}

/** Return TRUE for IPv4-mapped address. */
#define ffip6_isv4mapped(adr)  IN6_IS_ADDR_V4MAPPED(&(adr)->ip6.sin6_addr)

/** Convert IPv4-mapped address to IPv4. */
static FFINL void ffip_v4mapped_tov4(ffaddr *a) {
	a->ip4.sin_port = a->ip6.sin6_port;
	ffip4_set(a, (struct in_addr*)(((byte*)&a->ip6.sin6_addr) + 12));
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
	socklen_t size = FFADDR_MAXLEN;
	if (0 != getsockname(sk, &adr->a, &size)
		|| size > FFADDR_MAXLEN)
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

/** Shift an array of ffiovec.
Return the number of shifted elements. */
FF_EXTN size_t ffiov_shiftv(ffiovec *iovs, size_t nels, uint64 *len);

/** Get the overall size of ffiovec[]. */
static FFINL size_t ffiov_size(const ffiovec *iovs, size_t nels)
{
	size_t sz = 0;
	size_t i;
	for (i = 0;  i < nels;  ++i) {
		sz += iovs[i].iov_len;
	}
	return sz;
}

static FFINL void ffsf_sethdtr(sf_hdtr *ht, ffiovec *hdrs, size_t nhdrs, ffiovec *trls, size_t ntrls) {
	ht->headers = hdrs;
	ht->hdr_cnt = (int)nhdrs;
	ht->trailers = trls;
	ht->trl_cnt = (int)ntrls;
}

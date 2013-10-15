/**
Copyright (c) 2013 Simon Zolin
*/

#include <ws2tcpip.h>
#include <mswsock.h>

#include <FFOS/error.h>

enum {
	FF_BADSKT = -1
	, FF_MAXIP6 = INET6_ADDRSTRLEN

	, SOCK_NONBLOCK = 0
};

FF_EXTN LPFN_ACCEPTEX _ffwsaAcceptEx;
FF_EXTN LPFN_CONNECTEX _ffwsaConnectEx;
FF_EXTN LPFN_DISCONNECTEX _ffwsaDisconnectEx;
FF_EXTN LPFN_GETACCEPTEXSOCKADDRS _ffwsaGetAcceptExSockaddrs;
FF_EXTN int _ffwsaGetFuncs();

static FFINL int ffskt_init(int flags) {
	if (flags & FFSKT_WSA) {
		WSADATA wsa;
		if (0 != WSAStartup(MAKEWORD(2, 2), &wsa))
			return -1;
	}
	if ((flags & FFSKT_WSAFUNCS) && 0 != _ffwsaGetFuncs())
		return -1;
	return 0;
}

/** Does not inherit non-blocking mode. */
FF_EXTN ffskt ffskt_accept(ffskt listenSk, struct sockaddr *a, socklen_t *addrSize, int flags);

static FFINL ssize_t ffskt_recv(ffskt sk, void *buf, size_t size, int flags) {
	return recv(sk, (char *)buf, FF_TOINT(size), flags);
}

static FFINL ssize_t ffskt_send(ffskt sk, const void *buf, size_t size, int flags) {
	return send(sk, (const char *)buf, FF_TOINT(size), flags);
}


typedef WSABUF ffiovec;

/** Accessors for ffiovec. */
#define iov_base  buf
#define iov_len  len

static FFINL ssize_t ffskt_sendv(ffskt sk, ffiovec *ar, int arlen) {
	DWORD sent;
	if (0 == WSASend(sk, ar, arlen, &sent, 0, NULL, NULL))
		return sent;
	return -1;
}


//note: usual function shutdown() fails with WSAENOTCONN on a connected and valid socket assigned to IOCP
#define ffskt_fin(sk)  (0 == _ffwsaDisconnectEx(sk, (OVERLAPPED*)NULL, 0, 0))

#define ffskt_close  closesocket

static FFINL int ffskt_nblock(ffskt sk, int nblock) {
	return ioctlsocket(sk, FIONBIO, (unsigned long *)&nblock);
}


typedef ADDRINFOT ffaddrinfo;

static FFINL int ffaddr_info(ffaddrinfo **a, const ffsyschar *host, const ffsyschar *svc
	, int flags, int sockType, int proto)
{
	ffaddrinfo hints = { 0 };
	hints.ai_flags = flags;
	hints.ai_socktype = sockType;
	hints.ai_protocol = proto;
	return GetAddrInfo(host, svc, &hints, a);
}

#define ffaddr_free  FreeAddrInfo

#define ffaddr_name(a, addrlen, host, hostcap, svc, svccap, flags)\
	GetNameInfoA(a, addrlen, host, FF_TOINT(hostcap), svc, FF_TOINT(svccap), flags)

#define ffip_v6equal  IN6_ADDR_EQUAL

/**
Copyright (c) 2013 Simon Zolin
*/

#include <ws2tcpip.h>
#include <mswsock.h>

#include <FFOS/error.h>


#define FF_BADSKT  ((SOCKET)-1)

enum {
	FF_MAXIP4 = INET_ADDRSTRLEN
	, FF_MAXIP6 = INET6_ADDRSTRLEN

	, SOCK_NONBLOCK = 0x100
};

FF_EXTN ffskt ffskt_create(uint domain, uint type, uint protocol);

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

FF_EXTN ffskt ffskt_accept(ffskt listenSk, struct sockaddr *a, socklen_t *addrSize, int flags);

#define ffskt_deferaccept(sk, enable)  (-1)

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

typedef struct sf_hdtr {
	ffiovec *headers;
	int hdr_cnt;
	ffiovec *trailers;
	int trl_cnt;
} sf_hdtr;


//note: usual function shutdown() fails with WSAENOTCONN on a connected and valid socket assigned to IOCP
static FFINL int ffskt_fin(ffskt sk) {
	return 0 == _ffwsaDisconnectEx(sk, (OVERLAPPED*)NULL, 0, 0);
}

#define ffskt_close  closesocket

static FFINL int ffskt_nblock(ffskt sk, int nblock) {
	return ioctlsocket(sk, FIONBIO, (unsigned long *)&nblock);
}


typedef ADDRINFOT ffaddrinfo;

static FFINL int ffaddr_infoq(ffaddrinfo **a, const ffsyschar *host, const ffsyschar *svc, int flags)
{
	ffaddrinfo hints = { 0 };
	hints.ai_flags = flags;
	return GetAddrInfo(host, svc, &hints, a);
}

FF_EXTN int ffaddr_info(ffaddrinfo **a, const char *host, const char *svc, int flags);

#define ffaddr_free  FreeAddrInfo

#define ffaddr_nameq(a, addrlen, host, hostcap, svc, svccap, flags) \
	GetNameInfoW(a, addrlen, host, FF_TOINT(hostcap), svc, FF_TOINT(svccap), flags)

FF_EXTN int ffaddr_name(struct sockaddr *a, size_t addrlen, char *host, size_t hostcap, char *svc, size_t svccap, uint flags);

static FFINL const char * ffaddr_errstr(int code, char *buf, size_t bufcap) {
	fferr_str(code, buf, bufcap);
	return buf;
}

#define ffip6_eq  IN6_ADDR_EQUAL

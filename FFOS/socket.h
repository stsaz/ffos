/** ffos: socket
2020, Simon Zolin
*/

/*
Address:
	ffsockaddr_set_ipv4 ffsockaddr_set_ipv6
	ffsockaddr_ip_port
Creation:
	ffsock_create ffsock_create_tcp ffsock_create_udp
	ffsock_accept ffsock_accept_async
	ffsock_close
Configuration:
	ffsock_nonblock
	ffsock_setopt ffsock_deferaccept
	ffsock_getopt
	ffsock_bind
	ffsock_connect ffsock_connect_async
	ffsock_listen
I/O:
	ffsock_recv ffsock_recvfrom
	ffsock_send ffsock_sendv ffsock_sendto
Async I/O:
	ffsock_recv_async ffsock_recv_udp_async ffsock_recvfrom_async
	ffsock_send_async ffsock_sendv_async
I/O vector:
	ffiovec_set
	ffiovec_get
	ffiovec_shift
*/

#pragma once

#include <FFOS/string.h>
#include <FFOS/kqtask.h>
#include <ffbase/slice.h>

#ifdef FF_WIN
	#include <ws2tcpip.h>
	#include <mswsock.h>
#else
	#include <netinet/in.h>
	#include <netinet/tcp.h>
	#include <sys/socket.h>
#endif

typedef struct ffsockaddr {
	ffuint len;
	union {
		struct sockaddr_in ip4;
		struct sockaddr_in6 ip6;
	};
} ffsockaddr;

/** Set IPv4 address and L4 port
ipv4: byte[4]
  NULL: "0.0.0.0" */
static inline void ffsockaddr_set_ipv4(ffsockaddr *a, const void *ipv4, ffuint port)
{
	a->len = sizeof(struct sockaddr_in);
	a->ip4.sin_family = AF_INET;
	a->ip4.sin_port = ffint_be_cpu16(port);

	if (ipv4 != NULL)
		*(ffuint*)&a->ip4.sin_addr = *(ffuint*)ipv4;
	else
		*(ffuint*)&a->ip4.sin_addr = 0;
}

/** Set IPv6 address and L4 port
ipv6: byte[16]
  NULL: "::" */
static inline void ffsockaddr_set_ipv6(ffsockaddr *a, const void *ipv6, ffuint port)
{
	a->len = sizeof(struct sockaddr_in6);
	a->ip6.sin6_family = AF_INET6;
	a->ip6.sin6_port = ffint_be_cpu16(port);

	if (ipv6 != NULL)
		ffmem_copy(&a->ip6.sin6_addr, ipv6, 16);
	else
		ffmem_fill(&a->ip6.sin6_addr, 0x00, 16);
}

/** Get IP address (IPv4 or IPv6) and L4 port */
static inline ffslice ffsockaddr_ip_port(const ffsockaddr *a, ffuint *port)
{
	ffslice s = {};
	if (a->ip4.sin_family == AF_INET) {
		ffslice_set(&s, &a->ip4.sin_addr, 4);
		*port = ffint_be_cpu16(a->ip4.sin_port);

	} else if (a->ip4.sin_family == AF_INET6) {
		ffslice_set(&s, &a->ip6.sin6_addr, 16);
		*port = ffint_be_cpu16(a->ip6.sin6_port);
	}
	return s;
}

enum FFSOCK_INIT {
	FFSOCK_INIT_SIGPIPE = 1, // UNIX: ignore SIGPIPE.  Use once per process.
	FFSOCK_INIT_WSA = 2, // Windows: initialize WSA library.  Use once per process.
	FFSOCK_INIT_WSAFUNCS = 4, // Windows: get WSA functions.  Use once per module.
};


#ifdef FF_WIN

typedef SOCKET ffsock;
typedef WSABUF ffiovec;
#define FFSOCK_NULL  INVALID_SOCKET
#define FFSOCK_NONBLOCK  0x0100
#define FFSOCK_EINPROGRESS  ERROR_IO_PENDING
FF_EXTERN LPFN_DISCONNECTEX _ff_DisconnectEx;
FF_EXTERN LPFN_CONNECTEX _ff_ConnectEx;
FF_EXTERN LPFN_ACCEPTEX _ff_AcceptEx;
FF_EXTERN LPFN_GETACCEPTEXSOCKADDRS _ff_GetAcceptExSockaddrs;

static inline void ffsock_close(ffsock sk)
{
	if (sk == FFSOCK_NULL) return;
	closesocket(sk);
}

static inline int ffsock_nonblock(ffsock sk, int nonblock)
{
	return ioctlsocket(sk, FIONBIO, (unsigned long*)&nonblock);
}

static inline int ffsock_setopt(ffsock sk, int level, int name, int val)
{
	return setsockopt(sk, level, name, (char*)&val, sizeof(int));
}

static inline int ffsock_getopt(ffsock sk, int level, int name, int *dst)
{
	socklen_t len = sizeof(int);
	return getsockopt(sk, level, name, (char*)dst, &len);
}

static inline int ffsock_deferaccept(ffsock sk, int enable)
{
	(void)sk; (void)enable;
	SetLastError(ERROR_NOT_SUPPORTED);
	return -1;
}

static inline ffsock ffsock_create(int domain, int type, int protocol)
{
	ffsock sk = socket(domain, type & ~FFSOCK_NONBLOCK, protocol);

	if ((type & FFSOCK_NONBLOCK) && sk != FFSOCK_NULL
		&& 0 != ffsock_nonblock(sk, 1)) {
		ffsock_close(sk);
		return FFSOCK_NULL;
	}

	return sk;
}

static inline int ffsock_connect(ffsock sk, const ffsockaddr *addr)
{
	if (0 != connect(sk, (struct sockaddr*)&addr->ip4, addr->len))
		return -1;
	return 0;
}

static inline int ffsock_connect_async(ffsock sk, const ffsockaddr *addr, ffkq_task *task)
{
	if (task->active) {
		DWORD res;
		if (!GetOverlappedResult(NULL, &task->overlapped, &res, 0)) {
			if (GetLastError() == ERROR_IO_INCOMPLETE)
				SetLastError(ERROR_IO_PENDING);
			else
				task->active = 0;
			return -1;
		}

		task->active = 0;
		return 0;
	}

	ffsockaddr a = *addr;
	a.ip4.sin_port = 0;
	if (0 != bind(sk, (struct sockaddr*)&a.ip4, a.len))
		return -1;

	ffmem_zero_obj(task);
	BOOL ok = _ff_ConnectEx(sk, (struct sockaddr*)&addr->ip4, addr->len, NULL, 0, NULL, &task->overlapped);
	if (ok)
		SetLastError(ERROR_IO_PENDING);
	else if (GetLastError() != ERROR_IO_PENDING)
		return -1;

	task->active = 1;
	return -1;
}

static inline ffsock ffsock_accept(ffsock listen_sk, ffsockaddr *peer, int flags)
{
	socklen_t addr_size = sizeof(struct sockaddr_in6);
	ffsock sk = accept(listen_sk, (struct sockaddr*)&peer->ip4, &addr_size);

	if ((flags & FFSOCK_NONBLOCK) && sk != FFSOCK_NULL
		&& 0 != ffsock_nonblock(sk, 1)) {
		ffsock_close(sk);
		return FFSOCK_NULL;
	}

	peer->len = addr_size;
	return sk;
}

static inline ffsock ffsock_accept_async(ffsock lsock, ffsockaddr *peer, int flags, int domain, ffsockaddr *local, ffkq_task_accept *task)
{
	DWORD res;
	if (task->active) {
		if (!GetOverlappedResult(NULL, &task->overlapped, &res, 0)) {
			if (GetLastError() == ERROR_IO_INCOMPLETE)
				SetLastError(ERROR_IO_PENDING);
			else
				task->active = 0;
			return INVALID_SOCKET;
		}

		int len_local = 0, len_peer = 0;
		struct sockaddr *addr_local, *addr_peer;
		_ff_GetAcceptExSockaddrs(task->local_peer_addrs, 0, sizeof(struct sockaddr_in6) + 16, sizeof(struct sockaddr_in6) + 16, &addr_local, &len_local, &addr_peer, &len_peer);
		peer->len = 0;
		if (domain == ((struct sockaddr_in*)addr_peer)->sin_family) {
			peer->len = len_peer;
			ffmem_copy(&peer->ip4, addr_peer, len_peer);
			if (local != NULL) {
				local->len = len_local;
				ffmem_copy(&local->ip4, addr_local, len_local);
			}
		}

		task->active = 0;
		return task->csock;
	}

	SOCKET csock;
	if (INVALID_SOCKET != (csock = ffsock_accept(lsock, peer, flags & FFSOCK_NONBLOCK)))
		return csock;
	if (GetLastError() != WSAEWOULDBLOCK)
		return INVALID_SOCKET;

	if (INVALID_SOCKET == (csock = ffsock_create(domain, SOCK_STREAM | (flags & FFSOCK_NONBLOCK), 0)))
		return INVALID_SOCKET;

	ffmem_zero_obj(task);
	BOOL ok = _ff_AcceptEx(lsock, csock, task->local_peer_addrs, 0, sizeof(struct sockaddr_in6) + 16, sizeof(struct sockaddr_in6) + 16, &res, &task->overlapped);
	if (ok) {
		SetLastError(ERROR_IO_PENDING);
	} else if (GetLastError() != ERROR_IO_PENDING) {
		closesocket(csock);
		return INVALID_SOCKET;
	}

	task->csock = csock;
	task->active = 1;
	return INVALID_SOCKET;
}

static inline ffssize ffsock_recv(ffsock sk, void *buf, ffsize cap, int flags)
{
	return recv(sk, (char*)buf, ffmin(cap, 0xffffffff), flags);
}

static inline ffssize ffsock_recvfrom(ffsock sk, void *buf, ffsize cap, int flags, ffsockaddr *peer_addr)
{
	socklen_t size = sizeof(struct sockaddr_in6);
	int r = recvfrom(sk, (char*)buf, cap, flags, (struct sockaddr*)&peer_addr->ip4, &size);
	if (r < 0)
		return r;
	peer_addr->len = size;
	return r;
}

static inline ffssize ffsock_recv_async(ffsock sk, void *buf, ffsize cap, ffkq_task *task)
{
	DWORD read;
	if (task->active) {
		if (!GetOverlappedResult(NULL, &task->overlapped, &read, 0)) {
			if (GetLastError() == ERROR_IO_INCOMPLETE)
				SetLastError(ERROR_IO_PENDING);
			else
				task->active = 0;
			return -1;
		}

		task->active = 0;
	}

	cap = ffmin(cap, 0xffffffff);
	int r = recv(sk, (char*)buf, cap, 0);
	if (!(r < 0 && GetLastError() == WSAEWOULDBLOCK))
		return r;

	ffmem_zero_obj(&task->overlapped);
	BOOL ok = ReadFile((HANDLE)sk, NULL, 0, &read, &task->overlapped);
	if (ok)
		SetLastError(ERROR_IO_PENDING);
	else if (GetLastError() != ERROR_IO_PENDING)
		return -1;

	task->active = 1;
	return -1;
}

static inline ffssize ffsock_recv_udp_async(ffsock sk, void *buf, ffsize cap, ffkq_task *task)
{
	DWORD read;
	if (task->active) {
		if (!GetOverlappedResult(NULL, &task->overlapped, &read, 0)) {
			if (GetLastError() == ERROR_IO_INCOMPLETE)
				SetLastError(ERROR_IO_PENDING);
			else
				task->active = 0;
			return -1;
		}

		task->active = 0;
		return read;
	}

	cap = ffmin(cap, 0xffffffff);
	int r = recv(sk, (char*)buf, cap, 0);
	if (!(r < 0 && GetLastError() == WSAEWOULDBLOCK))
		return r;

	ffmem_zero_obj(&task->overlapped);
	BOOL ok = ReadFile((HANDLE)sk, buf, cap, &read, &task->overlapped);
	if (ok)
		SetLastError(ERROR_IO_PENDING);
	else if (GetLastError() != ERROR_IO_PENDING)
		return -1;

	task->active = 1;
	return -1;
}

static inline ffssize ffsock_recvfrom_async(ffsock sk, void *buf, ffsize cap, ffsockaddr *peer_addr, ffkq_task *task)
{
	DWORD read;
	if (task->active) {
		if (!GetOverlappedResult(NULL, &task->overlapped, &read, 0)) {
			if (GetLastError() == ERROR_IO_INCOMPLETE)
				SetLastError(ERROR_IO_PENDING);
			else
				task->active = 0;
			return -1;
		}

		task->active = 0;
		return read;
	}

	cap = ffmin(cap, 0xffffffff);
	socklen_t addr_size = sizeof(struct sockaddr_in6);
	int r = recvfrom(sk, (char*)buf, cap, 0, (struct sockaddr*)&peer_addr->ip4, &addr_size);
	if (!(r < 0 && GetLastError() == WSAEWOULDBLOCK)) {
		peer_addr->len = addr_size;
		return r;
	}

	ffmem_zero_obj(&task->overlapped);
	peer_addr->len = sizeof(struct sockaddr_in6);
	WSABUF wb;
	wb.buf = (char*)buf;
	wb.len = cap;
	DWORD flags = 0;
	r = WSARecvFrom(sk, &wb, 1, &read, &flags, (SOCKADDR*)&peer_addr->ip4, (int*)&peer_addr->len, &task->overlapped, NULL);
	if (r == 0)
		SetLastError(ERROR_IO_PENDING);
	else if (GetLastError() != ERROR_IO_PENDING)
		return -1;

	task->active = 1;
	return -1;
}

static inline ffssize ffsock_send(ffsock sk, const void *buf, ffsize len, int flags)
{
	return send(sk, (const char*)buf, ffmin(len, 0xffffffff), flags);
}

static inline ffssize ffsock_sendv(ffsock sk, ffiovec *iov, ffuint iov_n)
{
	DWORD sent;
	if (0 == WSASend(sk, iov, iov_n, &sent, 0, NULL, NULL))
		return sent;
	return -1;
}

static inline ffssize ffsock_sendto(ffsock sk, const void *buf, ffsize len, int flags, const ffsockaddr *peer_addr)
{
	return sendto(sk, (char*)buf, ffmin(len, 0xffffffff), flags, (struct sockaddr*)&peer_addr->ip4, peer_addr->len);
}

static inline ffssize ffsock_send_async(ffsock sk, const void *buf, ffsize len, ffkq_task *task)
{
	DWORD sent;
	if (task->active) {
		if (!GetOverlappedResult(NULL, &task->overlapped, &sent, 0)) {
			if (GetLastError() == ERROR_IO_INCOMPLETE)
				SetLastError(ERROR_IO_PENDING);
			else
				task->active = 0;
			return -1;
		}

		task->active = 0;
		return sent;
	}

	len = ffmin(len, 0xffffffff);
	int r = send(sk, (const char*)buf, len, 0);
	if (!(r < 0 && GetLastError() == WSAEWOULDBLOCK))
		return r;

	ffmem_zero_obj(&task->overlapped);
	len = ffmin(len, sizeof(task->buf));
	ffmem_copy(task->buf, buf, len);
	BOOL ok = WriteFile((HANDLE)sk, task->buf, len, &sent, &task->overlapped);
	if (ok)
		SetLastError(ERROR_IO_PENDING);
	else if (GetLastError() != ERROR_IO_PENDING)
		return -1;

	task->active = 1;
	return -1;
}

static inline ffssize ffsock_sendv_async(ffsock sk, ffiovec *iov, ffuint iov_n, ffkq_task *task)
{
	DWORD sent;
	if (task->active) {
		if (!GetOverlappedResult(NULL, &task->overlapped, &sent, 0)) {
			if (GetLastError() == ERROR_IO_INCOMPLETE)
				SetLastError(ERROR_IO_PENDING);
			else
				task->active = 0;
			return -1;
		}

		task->active = 0;
		return sent;
	}

	if (0 == WSASend(sk, iov, iov_n, &sent, 0, NULL, NULL))
		return sent;
	else if (GetLastError() != WSAEWOULDBLOCK)
		return -1;

	// get the first bytes from the first non-empty iovec
	ffuint len = 0;
	for (ffuint i = 0;  i != iov_n;  i++) {
		if (iov[i].len != 0) {
			len = ffmin(iov[i].len, sizeof(task->buf));
			ffmem_copy(task->buf, iov[i].buf, len);
			break;
		}
	}
	if (len == 0)
		return 0;

	ffmem_zero_obj(&task->overlapped);
	BOOL ok = WriteFile((HANDLE)sk, task->buf, len, &sent, &task->overlapped);
	if (ok)
		SetLastError(ERROR_IO_PENDING);
	else if (GetLastError() != ERROR_IO_PENDING)
		return -1;

	task->active = 1;
	return -1;
}

/** Get WSA function pointer */
static inline void* _ff_wsa_getfunc(ffsock sk, const GUID *guid)
{
	void *func = NULL;
	DWORD b;
	WSAIoctl(sk, SIO_GET_EXTENSION_FUNCTION_POINTER, (void*)guid, sizeof(GUID), &func, sizeof(void*), &b, 0, 0);
	if (func == NULL)
		SetLastError(ERROR_PROC_NOT_FOUND);
	return func;
}

static inline int _ff_wsa_getfuncs()
{
	static const GUID guids[] = {
		WSAID_DISCONNECTEX,
		WSAID_CONNECTEX,
		WSAID_ACCEPTEX,
		WSAID_GETACCEPTEXSOCKADDRS,
	};
	static void** const funcs[] = {
		(void**)&_ff_DisconnectEx,
		(void**)&_ff_ConnectEx,
		(void**)&_ff_AcceptEx,
		(void**)&_ff_GetAcceptExSockaddrs,
	};

	int rc = 0;
	ffsock sk = ffsock_create(AF_INET, SOCK_STREAM, 0);
	if (sk == FFSOCK_NULL)
		return -1;

	for (ffuint i = 0;  i != FF_COUNT(guids);  i++) {
		if (NULL == (*funcs[i] = _ff_wsa_getfunc(sk, &guids[i]))) {
			rc = -1;
			break;
		}
	}

	ffsock_close(sk);
	return rc;
}

static inline int ffsock_init(int flags)
{
	if (flags & FFSOCK_INIT_WSA) {
		WSADATA wsa;
		if (0 != WSAStartup(MAKEWORD(2, 2), &wsa))
			return -1;
	}

	if (flags & FFSOCK_INIT_WSAFUNCS) {
		if (0 != _ff_wsa_getfuncs())
			return -1;
	}

	return 0;
}

static inline int ffsock_fin(ffsock sk)
{
	return 0 == _ff_DisconnectEx(sk, (OVERLAPPED*)NULL, 0, 0);
}


static inline void ffiovec_set(ffiovec *iov, const void *buf, ffsize len)
{
	iov->buf = (char*)buf;
	iov->len = ffmin(len, 0xffffffff);
}

static inline ffslice ffiovec_get(ffiovec *iov)
{
	ffslice s;
	ffslice_set(&s, iov->buf, iov->len);
	return s;
}

static inline ffsize ffiovec_shift(ffiovec *iov, ffsize n)
{
	n = ffmin(n, iov->len);
	iov->buf += n;
	iov->len -= n;
	return n;
}

#else // UNIX:

#include <sys/uio.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <signal.h>

typedef int ffsock;
typedef struct iovec ffiovec;
#define FFSOCK_NULL  (-1)
#define FFSOCK_EINPROGRESS  EINPROGRESS

#ifdef FF_APPLE
	#define FFSOCK_NONBLOCK  0x40000000
#else
	#define FFSOCK_NONBLOCK  SOCK_NONBLOCK
#endif

static inline int ffsock_init(int flags)
{
	if (flags & FFSOCK_INIT_SIGPIPE) {
		struct sigaction sa = {};
		sa.sa_handler = SIG_IGN,
		sigaction(SIGPIPE, &sa, NULL);
	}
	return 0;
}

static inline void ffsock_close(ffsock sk)
{
	if (sk == FFSOCK_NULL) return;
	close(sk);
}

static inline int ffsock_nonblock(ffsock sk, int nonblock)
{
	return ioctl(sk, FIONBIO, &nonblock);
}

static inline ffsock ffsock_create(int domain, int type, int protocol)
{
#ifdef FF_APPLE
	ffsock sk = socket(domain, type & ~FFSOCK_NONBLOCK, protocol);

	if ((type & FFSOCK_NONBLOCK) && sk != FFSOCK_NULL
		&& 0 != ffsock_nonblock(sk, 1)) {
		ffsock_close(sk);
		return FFSOCK_NULL;
	}

	return sk;

#else
	return socket(domain, type, protocol);
#endif
}

static inline int ffsock_connect(ffsock sk, const ffsockaddr *addr)
{
	if (0 != connect(sk, (struct sockaddr*)&addr->ip4, addr->len))
		return -1;
	return 0;
}

static inline int ffsock_connect_async(ffsock sk, const ffsockaddr *addr, ffkq_task *task)
{
	if (task->active) {
		task->active = 0;

#ifdef FF_LINUX
		if (task->kev_flags & 0x008 /*EPOLLERR*/) {
			int err;
			socklen_t len = 4;
			if (0 != getsockopt(sk, SOL_SOCKET, SO_ERROR, &err, &len))
				return -1;
			if (err != 0) {
				errno = err;
				return -1;
			}
		}
#else
		if ((task->kev_flags & 0x8000 /*EV_EOF*/) && task->kev_errno != 0) {
			errno = task->kev_errno;
			return -1;
		}
#endif

		return 0;
	}

	ffmem_zero_obj(task);
	if (0 == connect(sk, (struct sockaddr*)&addr->ip4, addr->len))
		return 0;

	task->active = (errno == EINPROGRESS);
	return -1;
}

static inline ffsock ffsock_accept(ffsock listen_sk, ffsockaddr *addr, int flags)
{
	socklen_t addr_size = sizeof(struct sockaddr_in6);

#if (defined FF_LINUX && !defined FF_ANDROID) || defined FF_BSD
	ffsock sk = accept4(listen_sk, (struct sockaddr*)&addr->ip4, &addr_size, flags);

#else
	ffsock sk = accept(listen_sk, (struct sockaddr*)&addr->ip4, &addr_size);

	if ((flags & FFSOCK_NONBLOCK) && sk != FFSOCK_NULL
		&& 0 != ffsock_nonblock(sk, 1)) {
		ffsock_close(sk);
		return FFSOCK_NULL;
	}

#endif

	addr->len = addr_size;
	return sk;
}

static int ffsock_localaddr(ffsock sk, ffsockaddr *addr);

static inline ffsock ffsock_accept_async(ffsock lsock, ffsockaddr *peer, int flags, int domain, ffsockaddr *local, ffkq_task_accept *task)
{
	(void)domain;
	task->active = 0;
	int sk;
	if (-1 == (sk = ffsock_accept(lsock, peer, flags & FFSOCK_NONBLOCK))) {
		if (errno == EAGAIN) {
			errno = EINPROGRESS;
			task->active = 1;
		}
		return -1;
	}

	if (local != NULL) {
		ffsock_localaddr(sk, local);
	}

	return sk;
}

static inline int ffsock_setopt(ffsock sk, int level, int name, int val)
{
	return setsockopt(sk, level, name, (void*)&val, sizeof(int));
}

static inline int ffsock_getopt(ffsock sk, int level, int name, int *dst)
{
	socklen_t len = sizeof(int);
	return getsockopt(sk, level, name, (void*)dst, &len);
}

static inline int ffsock_deferaccept(ffsock sk, int enable)
{
#if defined FF_LINUX
	return ffsock_setopt(sk, IPPROTO_TCP, TCP_DEFER_ACCEPT, enable);

#elif defined FF_BSD
	void *val = NULL;
	struct accept_filter_arg af;
	if (enable) {
		ffmem_zero_obj(&af);
		ffmem_copy(af.af_name, "dataready", FFS_LEN("dataready"));
		val = &af;
	}
	return setsockopt(sk, SOL_SOCKET, SO_ACCEPTFILTER, val, sizeof(struct accept_filter_arg));

#else
	(void)sk; (void)enable;
	errno = ENOSYS;
	return -1;
#endif
}

static inline ffssize ffsock_recv(ffsock sk, void *buf, ffsize cap, int flags)
{
	return recv(sk, (char*)buf, cap, flags);
}

static inline ffssize ffsock_recvfrom(ffsock sk, void *buf, ffsize cap, int flags, ffsockaddr *peer_addr)
{
	socklen_t size = sizeof(struct sockaddr_in6);
	int r = recvfrom(sk, buf, cap, flags, (struct sockaddr*)&peer_addr->ip4, &size);
	if (r < 0)
		return r;
	peer_addr->len = size;
	return r;
}

static inline ffssize ffsock_recv_async(ffsock sk, void *buf, ffsize cap, ffkq_task *task)
{
	ffssize r = recv(sk, buf, cap, 0);
	task->active = 0;
	if (r < 0 && errno == EAGAIN) {
		errno = EINPROGRESS;
		task->active = 1;
	}
	return r;
}

static inline ffssize ffsock_recv_udp_async(ffsock sk, void *buf, ffsize cap, ffkq_task *task)
{
	return ffsock_recv_async(sk, buf, cap, task);
}

static inline ffssize ffsock_recvfrom_async(ffsock sk, void *buf, ffsize cap, ffsockaddr *peer_addr, ffkq_task *task)
{
	socklen_t size = sizeof(struct sockaddr_in6);
	int r = recvfrom(sk, buf, cap, 0, (struct sockaddr*)&peer_addr->ip4, &size);
	task->active = 0;
	if (r < 0) {
		if (errno == EAGAIN) {
			errno = EINPROGRESS;
			task->active = 1;
		}
		return r;
	}
	peer_addr->len = size;
	return r;
}

static inline ffssize ffsock_send(ffsock sk, const void *buf, ffsize len, int flags)
{
	return send(sk, (char*)buf, len, flags);
}

static inline ffssize ffsock_sendv(ffsock sk, ffiovec *iov, ffuint iov_n)
{
	return writev(sk, iov, iov_n);
}

static inline ffssize ffsock_sendto(ffsock sk, const void *buf, ffsize len, int flags, const ffsockaddr *peer_addr)
{
	return sendto(sk, buf, len, flags, (struct sockaddr*)&peer_addr->ip4, peer_addr->len);
}

static inline ffssize ffsock_send_async(ffsock sk, const void *buf, ffsize len, ffkq_task *task)
{
	ffssize r = send(sk, buf, len, 0);
	task->active = 0;
	if (r < 0 && errno == EAGAIN) {
		errno = EINPROGRESS;
		task->active = 1;
	}
	return r;
}

static inline ffssize ffsock_sendv_async(ffsock sk, ffiovec *iov, ffuint iov_n, ffkq_task *task)
{
	ffssize r = writev(sk, iov, iov_n);
	task->active = 0;
	if (r < 0 && errno == EAGAIN) {
		errno = EINPROGRESS;
		task->active = 1;
	}
	return r;
}

static inline int ffsock_fin(ffsock sk)
{
	return shutdown(sk, SHUT_WR);
}


static inline void ffiovec_set(ffiovec *iov, const void *data, ffsize len)
{
	iov->iov_base = (char*)data;
	iov->iov_len = len;
}

static inline ffslice ffiovec_get(ffiovec *iov)
{
	ffslice s;
	ffslice_set(&s, iov->iov_base, iov->iov_len);
	return s;
}

static inline ffsize ffiovec_shift(ffiovec *iov, ffsize n)
{
	n = ffmin(n, iov->iov_len);
	iov->iov_base = (char*)iov->iov_base + n;
	iov->iov_len -= n;
	return n;
}

#endif

/** Skip/shift bytes in iovec array
Return 0 if there's nothing left */
static inline int ffiovec_array_shift(ffiovec *iov, ffsize n, ffsize skip)
{
	for (ffsize i = 0;  i != n;  i++) {
		skip -= ffiovec_shift(&iov[i], skip);
#ifdef FF_UNIX
		if (iov[i].iov_len != 0)
#else
		if (iov[i].len != 0)
#endif
			return 1;
	}
	return 0;
}


/** Prepare sockets for use
flags: enum FFSOCK_INIT
Return 0 on success */
static int ffsock_init(int flags);

/** Set option on a socket
Examples:
  SOL_SOCKET, SO_REUSEADDR, 1
  SOL_SOCKET, SO_RCVBUF, 1024
  IPPROTO_IPV6, IPV6_V6ONLY, 0
  IPPROTO_TCP, TCP_NODELAY, 1
  IPPROTO_TCP, TCP_NOPUSH, 1
Return 0 on success */
static int ffsock_setopt(ffsock sk, int level, int name, int val);

/** Set defer-accept option on a listening TCP socket
Linux and FreeBSD only
Return 0 on success */
static int ffsock_deferaccept(ffsock sk, int enable);

/** Get socket option
Return 0 on success */
static int ffsock_getopt(ffsock sk, int level, int name, int *dst);

/** Create an endpoint for communication
domain: AF_... e.g. AF_INET | AF_INET6
type: SOCK_... | FFSOCK_NONBLOCK
Return FFSOCK_NULL on error */
static ffsock ffsock_create(int domain, int type, int protocol);

/** Create TCP socket
domain: AF_... e.g. AF_INET | AF_INET6
flags: FFSOCK_NONBLOCK
Return FFSOCK_NULL on error */
static inline ffsock ffsock_create_tcp(int domain, int flags)
{
	return ffsock_create(domain, SOCK_STREAM | flags, IPPROTO_TCP);
}

/** Create UDP socket
domain: AF_... e.g. AF_INET | AF_INET6
flags: FFSOCK_NONBLOCK
Return FFSOCK_NULL on error */
static inline ffsock ffsock_create_udp(int domain, int flags)
{
	return ffsock_create(domain, SOCK_DGRAM | flags, IPPROTO_UDP);
}

/** Accept a connection on a socket
flags: FFSOCK_NONBLOCK
Return FFSOCK_NULL on error */
static ffsock ffsock_accept(ffsock listen_sk, ffsockaddr *addr, int flags);

/** Same as ffsock_accept(), except in case it can't complete immediately,
 it begins asynchronous operation and returns FFSOCK_NULL with error FFSOCK_EINPROGRESS.
Socket must be non-blocking.
flags: FFSOCK_NONBLOCK
domain: AF_INET | AF_INET6
local: Optional local address
*/
static ffsock ffsock_accept_async(ffsock lsock, ffsockaddr *peer, int flags, int domain, ffsockaddr *local, ffkq_task_accept *task);

/** Close a socket */
static void ffsock_close(ffsock sk);

/** Set non-blocking mode on a socket
Return 0 on success */
static int ffsock_nonblock(ffsock sk, int nonblock);

/** Bind a name to a socket
Return 0 on success */
static inline int ffsock_bind(ffsock sk, const ffsockaddr *addr)
{
	ffsock_setopt(sk, SOL_SOCKET, SO_REUSEADDR, 1);
	return bind(sk, (struct sockaddr*)&addr->ip4, addr->len);
}

/** Initiate a connection on a socket
Return 0 on success
  <0 on error */
static int ffsock_connect(ffsock sk, const ffsockaddr *addr);

/** Same as ffsock_connect(), except in case it can't complete immediately,
 it begins asynchronous operation and returns <0 with error FFSOCK_EINPROGRESS. */
static int ffsock_connect_async(ffsock sk, const ffsockaddr *addr, ffkq_task *task);

/** Listen for connections on a socket
Example:
  ffsock_listen(lsn_sk, SOMAXCONN);
Return 0 on success */
static inline int ffsock_listen(ffsock listen_sk, ffuint max_conn)
{
	return listen(listen_sk, max_conn);
}

/** Receive data from a socket
Return <0 on error */
static ffssize ffsock_recv(ffsock sk, void *buf, ffsize cap, int flags);

/** Receive data from a socket
Windows:
 Returns with WSAECONNRESET after the previous sendto() on this socket to an unreachable destination UDP port.
 This behaviour can be disabled by WSAIoctl(SIO_UDP_CONNRESET).
Return <0 on error */
static ffssize ffsock_recvfrom(ffsock sk, void *buf, ffsize cap, int flags, ffsockaddr *peer_addr);

/** Same as ffsock_recv(), except if it can't complete immediately,
 it begins asynchronous operation and returns <0 with error FFSOCK_EINPROGRESS.
Windows:
 For TCP sockets only! */
static ffssize ffsock_recv_async(ffsock sk, void *buf, ffsize cap, ffkq_task *task);

/** Same as ffsock_recv_async(), but for UDP sockets connected with ffsock_connect().
Windows:
 The `buf[0..cap]` region MUST STAY VALID until the signal from IOCP! */
static ffssize ffsock_recv_udp_async(ffsock sk, void *buf, ffsize cap, ffkq_task *task);

/** Same as ffsock_recvfrom(), except if it can't complete immediately,
 it begins asynchronous operation and returns <0 with error FFSOCK_EINPROGRESS.
Windows:
 The `buf[0..cap]` region and `peer_addr` MUST STAY VALID until the signal from IOCP! */
static ffssize ffsock_recvfrom_async(ffsock sk, void *buf, ffsize cap, ffsockaddr *peer_addr, ffkq_task *task);

/** Send data to a socket
Return <0 on error */
static ffssize ffsock_send(ffsock sk, const void *buf, ffsize len, int flags);

/** Send data from multiple buffers
Return <0 on error */
static ffssize ffsock_sendv(ffsock sk, ffiovec *iov, ffuint iov_n);

/** Send data to a socket
Return <0 on error */
static ffssize ffsock_sendto(ffsock sk, const void *buf, ffsize len, int flags, const ffsockaddr *peer_addr);

/** Same as ffsock_send()/ffsock_sendv(), except if it can't complete immediately,
 it begins asynchronous operation and returns <0 with error FFSOCK_EINPROGRESS.
Windows:
 The function creates the task which sends `buf[0]` byte before returning with FFSOCK_EINPROGRESS,
  otherwise we can't make IOCP signal us.
 On the next call it returns 1 as it should, but the new `buf[0]` byte is IGNORED!
 This means that if you wish to change the data to send AFTER the call - it's TOO LATE for that! */
static ffssize ffsock_send_async(ffsock sk, const void *buf, ffsize len, ffkq_task *task);
static ffssize ffsock_sendv_async(ffsock sk, ffiovec *iov, ffuint iov_n, ffkq_task *task);

/** Send TCP FIN
Windows:
 Requires ffsock_init(FFSOCK_INIT_WSAFUNCS) to get DisconnectEx() function pointer,
  because shutdown() fails with WSAENOTCONN on a connected and valid socket assigned to IOCP.
Return 0 on success */
static int ffsock_fin(ffsock sk);

/** Get local socket address
Return 0 on success */
static inline int ffsock_localaddr(ffsock sk, ffsockaddr *addr)
{
	socklen_t addr_size = sizeof(struct sockaddr_in6);
	if (0 != getsockname(sk, (struct sockaddr*)&addr->ip4, &addr_size)
		|| (ffuint)addr_size > sizeof(struct sockaddr_in6))
		return -1;
	addr->len = addr_size;
	return 0;
}


/** Set buffer */
static void ffiovec_set(ffiovec *iov, const void *data, ffsize len);

/** Get buffer */
static ffslice ffiovec_get(ffiovec *iov);

/** Shift offset
Return number of shifted bytes */
static ffsize ffiovec_shift(ffiovec *iov, ffsize n);

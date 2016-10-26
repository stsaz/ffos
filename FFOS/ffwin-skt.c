/** Windows socket functions.
Copyright (c) 2016 Simon Zolin
*/

#include <FFOS/asyncio.h>
#include <FFOS/socket.h>


LPFN_DISCONNECTEX _ffwsaDisconnectEx;
LPFN_CONNECTEX _ffwsaConnectEx;
LPFN_ACCEPTEX _ffwsaAcceptEx;
LPFN_GETACCEPTEXSOCKADDRS _ffwsaGetAcceptExSockaddrs;

typedef void (*wsafunc_t)(void);

static wsafunc_t* const _wsafuncs[] = {
	(wsafunc_t*)&_ffwsaDisconnectEx, (wsafunc_t*)&_ffwsaConnectEx,
	(wsafunc_t*)&_ffwsaAcceptEx, (wsafunc_t*)&_ffwsaGetAcceptExSockaddrs,
};
static const GUID _guids[] = {
	WSAID_DISCONNECTEX, WSAID_CONNECTEX,
	WSAID_ACCEPTEX, WSAID_GETACCEPTEXSOCKADDRS,
};

static FFINL wsafunc_t wsa_getfunc(ffskt sk, const GUID *guid)
{
	wsafunc_t func = NULL;
	DWORD b;
	WSAIoctl(sk, SIO_GET_EXTENSION_FUNCTION_POINTER
		, (void*)guid, sizeof(GUID), &func, sizeof(wsafunc_t), &b, 0, 0);
	if (func == NULL)
		fferr_set(ERROR_PROC_NOT_FOUND);
	return func;
}

int _ffwsaGetFuncs(void)
{
	int rc = 0;
	uint i;
	ffskt sk;

	sk = ffskt_create(AF_INET, SOCK_STREAM, 0);
	if (sk == FF_BADSKT)
		return -1;

	for (i = 0;  i != FFCNT(_guids);  i++) {
		if (NULL == (*_wsafuncs[i] = wsa_getfunc(sk, &_guids[i]))) {
			rc = -1;
			break;
		}
	}
	ffskt_close(sk);
	return rc;
}


int ffaddr_info(ffaddrinfo **a, const char *host, const char *svc, int flags)
{
	ffsyschar whost[NI_MAXHOST], wsvc[NI_MAXSERV];
	const ffsyschar *phost = NULL, *psvc = NULL;

	if (host != NULL) {
		if (0 == ff_utow(whost, FFCNT(whost), host, -1, 0))
			return -1;
		phost = whost;
	}

	if (svc != NULL) {
		if (0 == ff_utow(wsvc, FFCNT(wsvc), svc, -1, 0))
			return -1;
		psvc = wsvc;
	}

	return ffaddr_infoq(a, phost, psvc, flags);
}


static int _ffaio_acceptbegin(ffaio_acceptor *acc)
{
	ffskt sk;
	BOOL b;
	DWORD r;

	sk = ffskt_create(acc->family, acc->sktype, 0);
	if (sk == FF_BADSKT)
		return FFAIO_ERROR;

	ffmem_tzero(&acc->kev.ovl);
	b = _ffwsaAcceptEx(acc->kev.sk, sk, acc->addrs, 0, FFADDR_MAXLEN + 16, FFADDR_MAXLEN + 16, &r, &acc->kev.ovl);
	if (0 != fferr_ioret(b)) {
		int er = fferr_last();
		(void)ffskt_close(sk);
		fferr_set(er);
		return FFAIO_ERROR;
	}

	acc->csk = sk;
	return FFAIO_ASYNC;
}

static FFINL void getNAddrs(byte *addrs, ffaddr *local, ffaddr *peer)
{
	int lenP = 0, lenL = 0;
	struct sockaddr *lo, *pe;
	_ffwsaGetAcceptExSockaddrs(addrs, 0, FFADDR_MAXLEN + 16, FFADDR_MAXLEN + 16, &lo, &lenL, &pe, &lenP);

	memcpy(&local->a, lo, lenL);
	local->len = lenL;
	memcpy(&peer->a, pe, lenP);
	peer->len = lenP;
}

ffskt ffaio_accept(ffaio_acceptor *acc, ffaddr *local, ffaddr *peer, int flags, ffkev_handler handler)
{
	ffskt sk = FF_BADSKT;
	int er = 0;

	if (acc->csk == FF_BADSKT) {
		peer->len = FFADDR_MAXLEN;
		sk = ffskt_accept(acc->kev.sk, &peer->a, &peer->len, flags);
		if (sk != FF_BADSKT)
			(void)ffskt_local(sk, local);

		else if (fferr_again(fferr_last()) && FFAIO_ASYNC == _ffaio_acceptbegin(acc)) {
			acc->kev.handler = handler;
			fferr_set(WSAEWOULDBLOCK);
		}

		return sk;
	}

	if (0 == ffio_result(&acc->kev.ovl)) {
		sk = acc->csk;
		if (flags & SOCK_NONBLOCK)
			(void)ffskt_nblock(sk, 1);

		getNAddrs(acc->addrs, local, peer);

	} else {
		er = fferr_last();
		(void)ffskt_close(acc->csk);
	}

	acc->csk = FF_BADSKT;
	ffmem_zero(acc->addrs, sizeof(acc->addrs));

	if (er != 0)
		fferr_set(er);
	return sk;
}

int ffaio_connect(ffaio_task *t, ffaio_handler handler, const struct sockaddr *addr, socklen_t addr_size)
{
	BOOL b;
	DWORD r;
	ffaddr a;

	if (t->wpending) {
		t->wpending = 0;
		return _ffaio_result(t);
	}

	ffaddr_init(&a);
	ffaddr_setany(&a, addr->sa_family);
	if (0 != ffskt_bind(t->sk, &a.a, a.len))
		goto fail;

	ffmem_tzero(&t->wovl);
	b = _ffwsaConnectEx(t->sk, addr, addr_size, NULL, 0, &r, &t->wovl);

	if (0 != fferr_ioret(b))
		goto fail;

	t->whandler = handler;
	t->wpending = 1;
	FFDBG_PRINTLN(FFDBG_KEV | 9, "task:%p, fd:%L, rhandler:%p, whandler:%p\n"
		, t, (size_t)t->fd, t->rhandler, t->whandler);
	return FFAIO_ASYNC;

fail:
	return FFAIO_ERROR;
}

int ffaio_recv(ffaio_task *t, ffaio_handler handler, void *d, size_t cap)
{
	BOOL b;
	DWORD rd;
	ssize_t r;

	if (t->rpending) {
		t->rpending = 0;
		r = _ffaio_result(t);
		if (!(!t->udp && r == 0))
			return r;
	}

	r = ffskt_recv(t->sk, d, cap, 0);
	if (!(r < 0 && fferr_again(fferr_last())))
		return r;

	if (!t->udp) {
		/* For TCP socket we just wait until input data is available (or until peer sends FIN),
		 but for UDP socket the whole packet must be read at once, so the buffer must be valid until reading is done. */
		d = NULL;
		cap = 0;
	}

	ffmem_tzero(&t->rovl);
	b = ReadFile(t->fd, d, FF_TOINT(cap), &rd, &t->rovl);

	if (0 != fferr_ioret(b)) {
		return FFAIO_ERROR;
	}

	t->rhandler = handler;
	t->rpending = 1;
	FFDBG_PRINTLN(FFDBG_KEV | 9, "task:%p, fd:%L, rhandler:%p, whandler:%p\n"
		, t, (size_t)t->fd, t->rhandler, t->whandler);
	return FFAIO_ASYNC;
}

int ffaio_send(ffaio_task *t, ffaio_handler handler, const void *d, size_t len)
{
	BOOL b;
	DWORD wr;
	ssize_t r;

	if (t->wpending) {
		t->wpending = 0;
		if (0 != _ffaio_result(t))
			return FFAIO_ERROR;
	}

	r = ffskt_send(t->sk, d, len, 0);
	if (!(r < 0 && fferr_again(fferr_last())))
		return r;

	t->sendbuf[0] = *(byte*)d;
	ffmem_tzero(&t->wovl);
	b = WriteFile(t->fd, t->sendbuf, 1, &wr, &t->wovl);

	if (0 != fferr_ioret(b)) {
		return FFAIO_ERROR;
	}

	t->whandler = handler;
	t->wpending = 1;
	FFDBG_PRINTLN(FFDBG_KEV | 9, "task:%p, fd:%L, rhandler:%p, whandler:%p\n"
		, t, (size_t)t->fd, t->rhandler, t->whandler);
	return FFAIO_ASYNC;
}

int ffaio_sendv(ffaio_task *t, ffaio_handler handler, ffiovec *iovs, size_t iovcnt)
{
	BOOL b;
	DWORD wr;
	ssize_t r;

	if (t->wpending) {
		t->wpending = 0;
		if (0 != _ffaio_result(t))
			return FFAIO_ERROR;
	}

	r = ffskt_sendv(t->sk, iovs, iovcnt);
	if (!(r < 0 && fferr_again(fferr_last())))
		return r;

	ffiovec *v;
	// skip empty iovec's
	for (v = iovs;  v->iov_len == 0;  v++) {
	}
	t->sendbuf[0] = *(byte*)v->iov_base;
	ffmem_tzero(&t->wovl);
	b = WriteFile(t->fd, t->sendbuf, 1, &wr, &t->wovl);

	if (0 != fferr_ioret(b)) {
		return FFAIO_ERROR;
	}

	t->whandler = handler;
	t->wpending = 1;
	FFDBG_PRINTLN(FFDBG_KEV | 9, "task:%p, fd:%L, rhandler:%p, whandler:%p\n"
		, t, (size_t)t->fd, t->rhandler, t->whandler);
	return FFAIO_ASYNC;
}

int ffaio_cancelasync(ffaio_task *t, int op, ffaio_handler oncancel)
{
	if ((op & FFAIO_READ) && t->rhandler != NULL) {
		if (oncancel != NULL)
			t->rhandler = oncancel;
		CancelIoEx(t->fd, &t->rovl);
	}

	if ((op & FFAIO_WRITE) && t->whandler != NULL) {
		if (oncancel != NULL)
			t->whandler = oncancel;
		CancelIoEx(t->fd, &t->wovl);
	}

	t->canceled = 1;
	return 0;
}

int _ffaio_events(ffaio_task *t, const ffkqu_entry *e)
{
	int ev = 0;

	if (e->lpOverlapped == &t->wovl)
		ev |= FFKQU_WRITE;
	else
		ev |= FFKQU_READ;

	return ev;
}

int _ffaio_result(ffaio_task *t)
{
	int r;

	if (t->canceled) {
		fferr_set(ECANCELED);

		if (t->rhandler == NULL && t->whandler == NULL)
			t->canceled = 0; //reset flag only if both handlers have signaled
		r = -1;
		goto done;
	}

	r = ffio_result(t->ev->lpOverlapped);

done:
	t->ev = NULL;
	return r;
}
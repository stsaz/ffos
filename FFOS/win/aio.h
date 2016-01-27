/**
Copyright (c) 2013 Simon Zolin
*/


#define ffaio_fctxinit()  (0)
#define ffaio_fctxclose()

struct ffaio_filetask {
	ffkevent kev;
};

#define ffaio_fattach(ft, kq) \
	ffkqu_attach(kq, (ft)->kev.fd, ffkev_ptr(&(ft)->kev), 0)


typedef struct ffaio_acceptor {
	ffkevent kev;
	int family;
	int sktype;

	ffskt csk;
	byte addrs[(FFADDR_MAXLEN + 16) * 2];
} ffaio_acceptor;

static FFINL int ffaio_acceptinit(ffaio_acceptor *acc, fffd kq, ffskt lsk, void *udata, int family, int sktype)
{
	ffkev_init(&acc->kev);
	acc->kev.sk = lsk;
	acc->kev.udata = udata;

	acc->family = family;
	acc->sktype = sktype;
	acc->csk = FF_BADSKT;
	ffmem_zero(acc->addrs, sizeof(acc->addrs));
	return ffkqu_attach(kq, acc->kev.sk, ffkev_ptr(&acc->kev), 0);
}

static FFINL void ffaio_acceptfin(ffaio_acceptor *acc) {
	if (acc->csk != FF_BADSKT) {
		ffskt_close(acc->csk);
		acc->csk = FF_BADSKT;
	}
}

static FFINL int ffaio_result(ffaio_task *t) {
	int r = t->result;
	t->result = 0;
	return r;
}

FF_EXTN int ffaio_recv(ffaio_task *t, ffaio_handler handler, void *d, size_t cap);

FF_EXTN int ffaio_send(ffaio_task *t, ffaio_handler handler, const void *d, size_t len);

static FFINL int ffaio_sendv(ffaio_task *t, ffaio_handler handler, ffiovec *iovs, size_t iovcnt) {
	ffiovec v = {0};
	if (iovcnt != 0)
		v = iovs[0];
	return ffaio_send(t, handler, v.iov_base, v.iov_len);
}

FF_EXTN int _ffaio_events(ffaio_task *t, const ffkqu_entry *e);


/** Listen for inbound connections to a named pipe. */
static FFINL int ffaio_pipe_listen(ffkevent *kev, ffkev_handler handler)
{
	ffmem_tzero(&kev->ovl);
	BOOL b = ConnectNamedPipe(kev->fd, &kev->ovl);
	if (0 != fferr_ioret(b))
		return -1;
	if (!b)
		kev->handler = handler;
	return 0;
}

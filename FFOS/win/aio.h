/**
Copyright (c) 2013 Simon Zolin
*/

typedef struct ffaio_acceptor {
	ffaio_task task;
	int family;
	int sktype;

	ffskt csk;
	byte addrs[(FFADDR_MAXLEN + 16) * 2];
} ffaio_acceptor;

static FFINL int ffaio_acceptinit(ffaio_acceptor *acc, fffd kq, ffskt lsk, void *udata, int family, int sktype)
{
	ffaio_init(&acc->task);
	acc->task.sk = lsk;
	acc->task.udata = udata;

	acc->family = family;
	acc->sktype = sktype;
	acc->csk = FF_BADSKT;
	ffmem_zero(acc->addrs, sizeof(acc->addrs));
	return ffaio_attach(&acc->task, kq, FFKQU_READ);
}

FF_EXTN int ffaio_acceptbegin(ffaio_acceptor *acc, ffaio_handler handler);

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

FF_EXTN int _ffaio_events(ffaio_task *t, const ffkqu_entry *e);

/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/string.h>
#include <FFOS/socket.h>
#include <FFOS/asyncio.h>
#include <FFOS/mem.h>
#include <FFOS/test.h>

#define x FFTEST_BOOL

enum {
	NDATA = 1 * 1024 * 1024
};

typedef struct {
	ffskt csk;
	ffaio_task task;
	char ch;
	size_t nsent;
} conn_t;

typedef struct {
	ffskt lsk;
	ffaio_acceptor acc;
} srv_t;

typedef struct {
	ffskt sk;
	ffaio_task task;
	size_t nrecvd;
} cli_t;

static int quit;
static ffaddr adr;
static fffd kq;
static cli_t g_cli;

static void start_conn(conn_t *conn, ffaddr *a, int port, ffaio_handler func);
static void connect_onerr(void *udata);

static void serv_recv(void *udata)
{
	cli_t *c = udata;
	ssize_t r;
	char buf[64 * 1024];

	for (;;) {
		r = ffaio_recv(&c->task, &serv_recv, buf, sizeof(buf));
		if (r == FFAIO_ASYNC) {
			printf("server: receiving data..." FF_NEWLN);
			return;
		}

		if (r == 0)
			break;

		x(r > 0);
		c->nrecvd += r;
		printf("server: received data: +%d" FF_NEWLN, (int)r);
	}

	printf("server: done" FF_NEWLN);
	ffskt_close(c->sk);
	quit |= 1;
}

static void onaccept(cli_t *c)
{
	printf("server: accepted client" FF_NEWLN);
	ffaio_init(&c->task);
	c->task.sk = c->sk;
	c->task.udata = c;
	x(0 == ffaio_attach(&c->task, kq, FFKQU_READ));
	serv_recv(c);
}

static void do_accept(void *udata)
{
	srv_t *srv = udata;

	for (;;) {
		ffskt sk;
		ffaddr local
			, peer;

		sk = ffaio_accept(&srv->acc, &local, &peer, SOCK_NONBLOCK, &do_accept);

		if (sk == FF_BADSKT) {
			printf("server: accept() returned %d" FF_NEWLN, (int)fferr_last());

			if (fferr_again(fferr_last())) {
				break;

			} else if (fferr_fdlim(fferr_last())) {
				break;
			}
		}

		if (sk != FF_BADSKT) {
			cli_t *c = &g_cli;
			x(c != NULL);
			c->sk = sk;
			onaccept(c);
		}
	}
}

static void client_sendfile(void *udata)
{
#ifdef FF_UNIX
	conn_t *conn = udata;
	char buf[64 * 1024];

	sf_hdtr hdtr;
	ffiovec iovs[2];
	uint64 sent, sent2;
	fffd f;

	ffiov_set(&iovs[0], buf, 100);
	ffiov_set(&iovs[1], buf, 300);
	ffsf_sethdtr(&hdtr, &iovs[0], 1, &iovs[1], 1);

	f = fffile_open("/tmp/tmp-ffos", FFO_CREATE | FFO_TRUNC | FFO_RDWR);
	x(f != FF_BADFD);
	x(200 == fffile_write(f, buf, 200));

	x(0 == ffskt_sendfile(conn->csk, f, 0, fffile_size(f), &hdtr, &sent, 0));
	x(sent == 100 + 200 + 300);
	printf("client: sent %d" FF_NEWLN, (int)sent);

	//file size = 0
	x(0 == ffskt_sendfile(conn->csk, f, 0, 0, &hdtr, &sent2, 0));
	x(sent2 == 100 + 300);

	x(0 == fffile_close(f));
	x(0 == fffile_rm("/tmp/tmp-ffos"));

	printf("client: sent %d" FF_NEWLN, (int)sent2);
#endif
}

static void client_send(void *udata)
{
	conn_t *conn = udata;
	ssize_t r;
	char buf[64 * 1024];

	while (conn->nsent != NDATA) {
		r = ffaio_send(&conn->task, &client_send, buf, ffmin(NDATA - conn->nsent, sizeof(buf)));
		if (r == FFAIO_ASYNC) {
			printf("client: sending..." FF_NEWLN);
			conn->ch = buf[0];
			return;
		}

		x(r > 0);
		conn->nsent += r;
		printf("client: sent +%d" FF_NEWLN, (int)r);
	}

	printf("client: all sent" FF_NEWLN);
	quit |= 2;
	ffskt_close(conn->csk);
}

static void connect_onsig(void *udata)
{
	conn_t *conn = udata;
	if (conn->task.wpending)
		x(0 == ffaio_connect(&conn->task, NULL, NULL, 0));
	printf("client: connected" FF_NEWLN);
	client_sendfile(conn);
	client_send(conn);
}

static void connect_oncancel(void *udata)
{
	conn_t *conn = udata;
	if (conn->task.wpending)
		x(FFAIO_ERROR == ffaio_connect(&conn->task, NULL, NULL, 0));

#ifndef FF_BSD
	x(fferr_last() == ECANCELED);
#endif

	printf("client: connect aborted" FF_NEWLN);
	quit |= 4;
	(void)ffskt_close(conn->csk);

	start_conn(conn, &adr, 8081, &connect_onerr);
}

static void connect_onerr(void *udata)
{
	conn_t *conn = udata;
	if (conn->task.wpending)
		x(FFAIO_ERROR == ffaio_connect(&conn->task, NULL, NULL, 0));
	x(fferr_last() != 0);
	printf("client: connect error" FF_NEWLN);
	quit |= 8;
	(void)ffskt_close(conn->csk);

	start_conn(conn, &adr, 8080, &connect_onsig);
}

static int init_srv(srv_t *srv, ffaddr *a)
{
	srv->lsk = ffskt_create(AF_INET, SOCK_STREAM, 0);
	x(srv->lsk != FF_BADSKT);
	x(0 == ffskt_nblock(srv->lsk, 1));
	x(0 == ffskt_bind(srv->lsk, &a->a, a->len));
	x(0 == ffskt_listen(srv->lsk, SOMAXCONN));
	return 0;
}

static void start_conn(conn_t *conn, ffaddr *a, int port, ffaio_handler func)
{
	ffskt sk = ffskt_create(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	x(sk != FF_BADSKT);
	x(0 == ffskt_nblock(sk, 1));
	conn->csk = sk;

	ffaio_init(&conn->task);
	conn->task.sk = sk;
	conn->task.udata = conn;
	x(0 == ffaio_attach(&conn->task, kq, FFKQU_READ | FFKQU_WRITE));

	ffip_setport(a, port);
	printf("client: connecting to %d...\n", port);
	if (FFAIO_ASYNC == ffaio_connect(&conn->task, func, &a->a, a->len)) {
		return;
	}
	func(conn);
}


static void on_post(void *udata)
{
	ffkevent *ev = udata;
	ev->udata = (void*)-1;
}

static void test_kqu_post(void)
{
	fffd kq;
	ffkevpost post;
	uint n, i;
	ffkqu_entry ents[2];
	ffkqu_time t;
	ffkevent kev1, kev2;

	FFTEST_FUNC;

	kq = ffkqu_create();
	ffkqu_settm(&t, 0);

	x(0 == ffkqu_post_attach(&post, kq));

	x(0 == ffkqu_wait(kq, ents, FFCNT(ents), &t));

	ffkev_init(&kev1);
	ffkev_init(&kev2);
	kev1.handler = kev2.handler = &on_post;
	kev1.udata = &kev1;
	kev2.udata = &kev2;
	x(0 == ffkqu_post(&post, ffkev_ptr(&kev1)));
	x(0 == ffkqu_post(&post, ffkev_ptr(&kev2)));

	for (;;) {
		n = ffkqu_wait(kq, ents, FFCNT(ents), &t);

		if ((int)n < 0) {
			if (fferr_last() != EINTR) {
				x(0);
				break;
			}
			continue;
		}

		for (i = 0;  i != n;  i++) {
			ffkev_call(&ents[i]);
		}

#ifdef FF_WIN
		x(kev1.udata == (void*)-1);
		x(kev2.udata == (void*)-1);
#elif defined FF_BSD || defined FF_APPLE
		x(kev1.udata == &kev1);
		x(kev2.udata == (void*)-1);
#else
		x(kev1.udata == &kev1);
		x(kev2.udata == &kev2);
#endif

		x(0 == ffkqu_wait(kq, ents, FFCNT(ents), &t));
		break;
	}

	ffkqu_post_detach(&post, kq);
	ffkqu_close(kq);
}

int test_kqu()
{
	ffkqu_time tt;
	ffkqu_entry ents[4];
	int nevents;
	int n;
	srv_t srv;
	conn_t conn;

	FFTEST_FUNC;

	test_kqu_post();

	kq = ffkqu_create();
	x(kq != FF_BADFD);
	ffkqu_settm(&tt, -1);

	x(0 == ffskt_init(FFSKT_WSA | FFSKT_WSAFUNCS));

	{
		ffaddrinfo *a;
		x(0 == ffaddr_info(&a, "127.0.0.1", NULL, AI_PASSIVE));
		ffaddr_copy(&adr, a->ai_addr, a->ai_addrlen);
		ffaddr_free(a);
	}

	ffmem_tzero(&conn);
	ffmem_tzero(&srv);

	ffip_setport(&adr, 8080);
	init_srv(&srv, &adr);
	x(0 == ffaio_acceptinit(&srv.acc, kq, srv.lsk, &srv, AF_INET, SOCK_STREAM));
	do_accept(&srv);

	start_conn(&conn, &adr, 8081, &connect_oncancel);
	if (ffaio_active(&conn.task)) {
#if !defined FF_WIN || FF_WIN >= 0x0600
		ffaio_cancelasync(&conn.task, FFAIO_CONNECT, &connect_oncancel);
#endif
	}

	for (;;) {
		nevents = ffkqu_wait(kq, ents, FFCNT(ents), &tt);

		for (n = 0;  n < nevents;  n++) {
			ffkev_call(&ents[n]);
		}

		if (nevents == -1 && fferr_last() != EINTR) {
			x(0);
			break;
		}

		if (quit == 0x0f)
			break;
	}

	x(0 == ffkqu_close(kq));
	ffaio_acceptfin(&srv.acc);
	return 0;
}

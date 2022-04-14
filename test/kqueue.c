/** ffos: queue.h tester
2020, Simon Zolin
*/

#include <FFOS/queue.h>
#include <FFOS/pipe.h>
#include <FFOS/file.h>
#include <FFOS/socket.h>
#include <FFOS/test.h>

#if 0
/** Wait until the specified (read or write) event signals */
int kq_wait(ffkq kq, ffkq_event *events, ffuint events_cap, ffkq_time timeout, int rw)
{
	for (;;) {
		xint_sys(1, ffkq_wait(kq, events, events_cap, timeout));
		ffuint f = ffkq_event_flags(&events[0]);
		fflog("ffkq_event_flags(): %xu", f);
		if (f & rw)
			break;
	}
	return 1;
}

/*
. create 2 server/client kqueue objects
. create server/client sockets; attach them to kqueue
. connect and accept
. send and receive data between both peers
*/
void test_kqueue_socket()
{
	ffkq kq;
	kq = ffkq_create();
	x_sys(kq != FFKQ_NULL);

	ffkq kqc;
	kqc = ffkq_create();
	x_sys(kqc != FFKQ_NULL);

	char buf[100];
	ffkq_event ev;
	ffkq_time tm;
	ffkq_time_set(&tm, 1000);

	ffsock l = ffsock_create_tcp(AF_INET, FFSOCK_NONBLOCK);
	x_sys(l != FFSOCK_NULL);
	x_sys(0 == ffsock_setopt(l, IPPROTO_TCP, TCP_NODELAY, 1));

	ffsockaddr addr = {};
	ffsockaddr_set_ipv4(&addr, NULL, 0);
	x_sys(0 == ffsock_bind(l, &addr));
	x_sys(0 == ffsock_listen(l, SOMAXCONN));
	x_sys(0 == ffkq_attach_socket(kq, l, &l, FFKQ_READWRITE));

	x_sys(FFSOCK_NULL == ffsock_accept(l, &addr, 0));
	x_sys(fferr_again(fferr_last()));

	x_sys(0 == ffsock_localaddr(l, &addr));
	ffuint port = 0;
	ffslice ips = ffsockaddr_ip_port(&addr, &port);
	ffbyte *ip = (void*)ips.ptr;
	fflog("srv: listening on %u.%u.%u.%u port %d"
		, ip[0], ip[1], ip[2], ip[3]
		, port);

	ffsock c = ffsock_create_tcp(AF_INET, FFSOCK_NONBLOCK);
	x_sys(c != FFSOCK_NULL);
	x_sys(0 == ffsock_setopt(c, IPPROTO_TCP, TCP_NODELAY, 1));
	x_sys(0 == ffkq_attach_socket(kqc, c, &c, FFKQ_READWRITE));

	ffsockaddr_set_ipv4(&addr, "\x7f\x00\x00\x01", port);
	int r = ffsock_connect(c, &addr);
	fflog("cli: connect: %d", r);
	x_sys(r == 0 || errno == FFSOCK_EINPROGRESS);

	if (r == -1) {
		fflog("cli: waiting for connect...");
		xint_sys(1, kq_wait(kqc, &ev, 1, tm, FFKQ_WRITE));
		xieq((ffsize)&c, (ffsize)ffkq_event_data(&ev));
		xieq(0, ffkq_event_result_socket(&ev, c));
	}

	fflog("srv: waiting for accept...");
	xint_sys(1, kq_wait(kq, &ev, 1, tm, FFKQ_READ));
	xieq((ffsize)&l, (ffsize)ffkq_event_data(&ev));
	xieq(0, ffkq_event_result_socket(&ev, c));

	ffsock lc = ffsock_accept(l, &addr, FFSOCK_NONBLOCK);
	x_sys(lc != FFSOCK_NULL);
	x_sys(0 == ffsock_setopt(lc, IPPROTO_TCP, TCP_NODELAY, 1));
	x_sys(0 == ffkq_attach_socket(kq, lc, &lc, FFKQ_READWRITE));
	ips = ffsockaddr_ip_port(&addr, &port);
	ip = (void*)ips.ptr;
	fflog("srv: accepted connection from %u.%u.%u.%u port %d"
		, ip[0], ip[1], ip[2], ip[3]
		, port);

	x_sys(-1 == ffsock_recv(lc, buf, sizeof(buf), 0));
	x_sys(fferr_again(fferr_last()));

	ffssize n = ffsock_send(c, "hello", 5, 0);
	fflog("cli: send: %L", n);
	x_sys(n == 5 || (n == -1 && fferr_again(fferr_last())));

	if (n == -1) {
		fflog("cli: waiting for send...");
		xint_sys(1, kq_wait(kqc, &ev, 1, tm, FFKQ_WRITE));
		xieq((ffsize)&c, (ffsize)ffkq_event_data(&ev));
		xieq(0, ffkq_event_result_socket(&ev, c));
	}

	x_sys(-1 == ffsock_recv(c, buf, sizeof(buf), 0));
	x_sys(fferr_again(fferr_last()));

	fflog("srv: waiting for recv...");
	xint_sys(1, kq_wait(kq, &ev, 1, tm, FFKQ_READ));
	xieq((ffsize)&lc, (ffsize)ffkq_event_data(&ev));
	xieq(0, ffkq_event_result_socket(&ev, lc));

	n = ffsock_recv(lc, buf, sizeof(buf), 0);
	xint_sys(5, n);
	x(!ffmem_cmp(buf, "hello", 5));

	x_sys(-1 == ffsock_recv(lc, buf, sizeof(buf), 0));
	x_sys(fferr_again(fferr_last()));

	n = ffsock_send(c, "man", 3, 0);
	fflog("cli: send: %L", n);
	x_sys(n == 3);

	n = ffsock_send(lc, "data", 4, 0);
	fflog("srv: send: %L", n);
	x_sys(4 == n);

	fflog("cli: waiting for recv...");
	xint_sys(1, kq_wait(kqc, &ev, 1, tm, FFKQ_READ));
	xieq((ffsize)&c, (ffsize)ffkq_event_data(&ev));
	xieq(0, ffkq_event_result_socket(&ev, c));

	n = ffsock_recv(c, buf, sizeof(buf), 0);
	xint_sys(4, n);
	x(!ffmem_cmp(buf, "data", 4));
	x_sys(0 == ffsock_fin(c));
	ffsock_close(c);

	fflog("srv: waiting for data...");
	xint_sys(1, kq_wait(kq, &ev, 1, tm, FFKQ_READ));
	xieq((ffsize)&lc, (ffsize)ffkq_event_data(&ev));
	xieq(0, ffkq_event_result_socket(&ev, lc));

	x_sys(3 == ffsock_recv(lc, buf, sizeof(buf), 0));
	x(!ffmem_cmp(buf, "man", 3));

	n = ffsock_recv(lc, buf, sizeof(buf), 0);
	x_sys(n == 0 || (n == -1 && fferr_again(fferr_last())));

	if (n == -1) {
		fflog("srv: waiting for FIN...");
		xint_sys(1, kq_wait(kq, &ev, 1, tm, FFKQ_READ));
		xieq((ffsize)&lc, (ffsize)ffkq_event_data(&ev));
		xieq(0, ffkq_event_result_socket(&ev, lc));

		x_sys(0 == ffsock_recv(lc, buf, sizeof(buf), 0));
	}

	x_sys(0 == ffsock_fin(lc));

	ffsock_close(l);
	ffsock_close(lc);
	ffkq_close(kq);
	ffkq_close(kqc);
}
#endif

/*
. create kqueue
. create kqueue post event
. post 2 events
. process posted events
*/
void test_kqueue_post()
{
	ffkq kq;
	kq = ffkq_create();
	x_sys(kq != FFKQ_NULL);

	ffkq_time tm;
	ffkq_time_set(&tm, 0);
	ffkq_event ev;
	xint_sys(0, ffkq_wait(kq, &ev, 1, tm));

	ffkq_postevent post = ffkq_post_attach(kq, (void*)0x12345678);
	x_sys(post != FFKQ_NULL);
	x_sys(0 == ffkq_post(post, (void*)0x12345678));
	x_sys(0 == ffkq_post(post, (void*)0x12345679));

	fflog("waiting for posted event...");
	ffkq_time_set(&tm, 1000);
	xint_sys(1, ffkq_wait(kq, &ev, 1, tm));
#if defined FF_WIN
	xint_sys(0x12345678, (ffsize)ffkq_event_data(&ev));
	xint_sys(1, ffkq_wait(kq, &ev, 1, tm));
	xint_sys(0x12345679, (ffsize)ffkq_event_data(&ev));
#elif defined FF_LINUX
	xint_sys(0x12345678, (ffsize)ffkq_event_data(&ev));
#else
	xint_sys(0x12345679, (ffsize)ffkq_event_data(&ev));
#endif
	ffkq_post_consume(post);

	ffkq_time_set(&tm, 0);
	xint_sys(0, ffkq_wait(kq, &ev, 1, tm));

	x_sys(0 == ffkq_post(post, (void*)0x12345678));

	fflog("waiting for posted event...");
	ffkq_time_set(&tm, 1000);
	xint_sys(1, ffkq_wait(kq, &ev, 1, tm));
	xint_sys(0x12345678, (ffsize)ffkq_event_data(&ev));
	ffkq_post_consume(post);

	ffkq_time_set(&tm, 0);
	xint_sys(0, ffkq_wait(kq, &ev, 1, tm));

	ffkq_post_detach(post, kq);
	ffkq_close(kq);
}

void test_kqueue_pipe()
{
	ffkq kq;
	fffd l, lc, c;
	ffkq_task task = {};

	kq = ffkq_create();

	const char *name = FFPIPE_NAME_PREFIX "ffostest.pipe";
#ifdef FF_UNIX
	name = "/tmp/ffostest.unix";
	fffile_remove(name);
#endif

	x_sys(FFPIPE_NULL != (l = ffpipe_create_named(name, FFPIPE_ASYNC)));
	x_sys(0 == ffkq_attach(kq, l, (void*)1, FFKQ_READ));

// synchronous accept
	x_sys(FFPIPE_NULL != (c = ffpipe_connect(name)));
	x_sys(FFPIPE_NULL != (lc = ffpipe_accept_async(l, &task)));
	ffpipe_peer_close(lc);
	ffpipe_close(c);

// asynchronous accept
	x_sys(FFPIPE_NULL == (lc = ffpipe_accept_async(l, &task)));
	x_sys(fferr_last() == FFPIPE_EINPROGRESS);

	x_sys(FFPIPE_NULL != (c = ffpipe_connect(name)));

	ffkq_event ev;
	ffkq_time tm;
	ffkq_time_set(&tm, 1000);
	x_sys(1 == ffkq_wait(kq, &ev, 1, tm));
	x(ffkq_event_data(&ev) == (void*)1);

	x_sys(FFPIPE_NULL != (lc = ffpipe_accept_async(l, &task)));

	ffpipe_peer_close(lc);
	ffpipe_close(l);
	ffpipe_close(c);
	ffkq_close(kq);
#ifdef FF_UNIX
	fffile_remove(name);
#endif
}

void test_kqueue_socket_accept()
{
	ffkq kq;
	ffsock l, lc, c;
	ffkq_task task = {};
	ffsockaddr peer, local;

	kq = ffkq_create();
	x_sys(FFSOCK_NULL != (l = ffsock_create_tcp(AF_INET, FFSOCK_NONBLOCK)));

	ffsockaddr a = {};
	char ip[] = {127,0,0,1};
	ffsockaddr_set_ipv4(&a, ip, 64000);
	x_sys(0 == ffsock_bind(l, &a));
	x_sys(0 == ffsock_listen(l, SOMAXCONN));
	x_sys(0 == ffkq_attach_socket(kq, l, &l, FFKQ_READ));

// asynchronous accept
	x_sys(FFSOCK_NULL == ffsock_accept_async(l, &peer, 0, AF_INET, &local, &task));
	x_sys(fferr_last() == FFSOCK_EINPROGRESS);

	x_sys(FFSOCK_NULL != (c = ffsock_create_tcp(AF_INET, 0)));
	x_sys(0 == ffsock_connect(c, &a));

	ffkq_event ev;
	ffkq_time tm;
	ffkq_time_set(&tm, 1000);
	x_sys(1 == ffkq_wait(kq, &ev, 1, tm));
	x(ffkq_event_data(&ev) == &l);

	x_sys(FFSOCK_NULL != (lc = ffsock_accept_async(l, &peer, 0, AF_INET, &local, &task)));
	ffuint port;
	ffslice ips = ffsockaddr_ip_port(&peer, &port);
	x(ffslice_eq(&ips, ip, 4, 1));
	ips = ffsockaddr_ip_port(&local, &port);
	x(ffslice_eq(&ips, ip, 4, 1));

	ffsock_close(lc);
	ffsock_close(c);

// synchronous accept
	x_sys(FFSOCK_NULL != (c = ffsock_create_tcp(AF_INET, 0)));
	x_sys(0 == ffsock_connect(c, &a));
#ifdef FF_WIN
	Sleep(500);
#endif
	x_sys(FFSOCK_NULL != (lc = ffsock_accept_async(l, &peer, 0, AF_INET, &local, &task)));

	ffsock_close(l);
	ffsock_close(lc);
	ffsock_close(c);
	ffkq_close(kq);
}

void test_kqueue_socket_connect()
{
	ffkq kq;
	ffsock l, c;
	ffkq_task task = {};

	kq = ffkq_create();

	x_sys(FFSOCK_NULL != (l = ffsock_create_tcp(AF_INET, 0)));
	ffsockaddr a = {};
	char ip[] = {127,0,0,1};
	ffsockaddr_set_ipv4(&a, ip, 64000);
	x_sys(0 == ffsock_bind(l, &a));
	x_sys(0 == ffsock_listen(l, SOMAXCONN));

// successful connection
	x_sys(FFSOCK_NULL != (c = ffsock_create_tcp(AF_INET, FFSOCK_NONBLOCK)));
	x_sys(0 == ffkq_attach_socket(kq, c, &c, FFKQ_WRITE));
	int r = ffsock_connect_async(c, &a, &task);
	x_sys(r == 0 || fferr_last() == FFSOCK_EINPROGRESS);

	ffkq_event ev;
	ffkq_time tm;
	ffkq_time_set(&tm, 1000);
	if (r != 0) {
		x_sys(1 == ffkq_wait(kq, &ev, 1, tm));
		x(ffkq_event_data(&ev) == &c);
		ffkq_task_event_assign(&task, &ev);
	}

	x_sys(0 == ffsock_connect_async(c, &a, &task));
	ffsock_close(c);

// failed connection
	x_sys(FFSOCK_NULL != (c = ffsock_create_tcp(AF_INET, FFSOCK_NONBLOCK)));
	x_sys(0 == ffkq_attach_socket(kq, c, &c, FFKQ_WRITE));

	ffsock_close(l);
	l = FFSOCK_NULL;

	x_sys(0 != ffsock_connect_async(c, &a, &task));
	x_sys(fferr_last() == FFSOCK_EINPROGRESS);

#ifdef FF_WIN
	ffsock_close(c);
	c = FFSOCK_NULL;
#endif

	x_sys(1 == ffkq_wait(kq, &ev, 1, tm));
	x(ffkq_event_data(&ev) == &c);
	ffkq_task_event_assign(&task, &ev);

	x_sys(0 != ffsock_connect_async(c, &a, &task));
	x_sys(fferr_last() != FFSOCK_EINPROGRESS);

	ffsock_close(l);
	ffsock_close(c);
	ffkq_close(kq);
}

void test_kqueue()
{
	test_kqueue_post();
	test_kqueue_pipe();
	x_sys(0 == ffsock_init(FFSOCK_INIT_SIGPIPE | FFSOCK_INIT_WSA | FFSOCK_INIT_WSAFUNCS));
	test_kqueue_socket_accept();
	test_kqueue_socket_connect();
#if 0
	test_kqueue_socket();
#endif
}

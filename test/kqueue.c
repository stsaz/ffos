/** ffos: queue.h tester
2020, Simon Zolin
*/

#include <FFOS/queue.h>
#include <FFOS/pipe.h>
#include <FFOS/file.h>
#include <FFOS/socket.h>
#include <FFOS/test.h>

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
	ffkq_task_accept task = {};
	ffsockaddr peer, local;
	ffkq_event ev;
	ffkq_time tm;
	ffkq_time_set(&tm, 1000);

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
	ffkq_event ev;
	ffkq_time tm;
	ffkq_time_set(&tm, 1000);

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

void test_kqueue_socket_io()
{
	ffkq kq = ffkq_create();
	ffsock l, lc, c;
	ffkq_task task = {};
	ffkq_event ev;
	ffkq_time tm;
	ffkq_time_set(&tm, 1000);

	// listen
	x_sys(FFSOCK_NULL != (l = ffsock_create_tcp(AF_INET, 0)));
	ffsockaddr a = {};
	char ip[] = {127,0,0,1};
	ffsockaddr_set_ipv4(&a, ip, 64000);
	x_sys(0 == ffsock_bind(l, &a));
	x_sys(0 == ffsock_listen(l, SOMAXCONN));

	// connect
	x_sys(FFSOCK_NULL != (c = ffsock_create_tcp(AF_INET, FFSOCK_NONBLOCK)));
	x_sys(!ffsock_setopt(c, SOL_SOCKET, SO_SNDBUF, 128*1024));
	x_sys(0 == ffkq_attach_socket(kq, c, &c, FFKQ_READWRITE));
	int r = ffsock_connect_async(c, &a, &task);
	x_sys(r == 0 || fferr_last() == FFSOCK_EINPROGRESS);

	x_sys(FFSOCK_NULL != (lc = ffsock_accept(l, &a, 0)));

	if (r != 0) {
		x_sys(1 == ffkq_wait(kq, &ev, 1, tm));
		x(ffkq_event_data(&ev) == &c);
		ffkq_task_event_assign(&task, &ev);
	}

	x_sys(0 == ffsock_connect_async(c, &a, &task));

	char cbuf[64000] = {};
	r = ffsock_recv_async(c, cbuf, sizeof(cbuf), &task);
	x_sys(r < 0 && fferr_last() == FFSOCK_EINPROGRESS);

	// server -> client
	x_sys(6 == ffsock_send(lc, "svdata", 6, 0));

	x_sys(1 == ffkq_wait(kq, &ev, 1, tm));
	x(ffkq_event_data(&ev) == &c);

	// client <- server
	r = ffsock_recv_async(c, cbuf, sizeof(cbuf), &task);
	x_sys(r == 6);
	x(!memcmp(cbuf, "svdata", 6));

	// client -> server
	// send until the system buffer is full
	int n = 0;
	memcpy(cbuf, "cldata", 6);
	for (;;) {
		r = ffsock_send_async(c, cbuf, sizeof(cbuf), &task);
		if (r < 0 && fferr_last() == FFSOCK_EINPROGRESS)
			break;
		x_sys(r > 0);
		n += r;
	}

	// server <- client
	// receive until the system buffer is empty
	char lbuf[64000];
	r = ffsock_recv(lc, lbuf, sizeof(lbuf), 0);
	x_sys(r >= 6);
	x(!memcmp(lbuf, "cldata", 6));
	n -= r;
	while (n > 0) {
		r = ffsock_recv(lc, lbuf, sizeof(lbuf), 0);
		x_sys(r > 0);
		n -= r;
	}

	x_sys(1 == ffkq_wait(kq, &ev, 1, tm));
	x(ffkq_event_data(&ev) == &c);

	// client -> server
	r = ffsock_send_async(c, "cldata", 6, &task);
	x_sys(r > 0);

	ffsock_close(l);
	ffsock_close(lc);
	ffsock_close(c);
	ffkq_close(kq);
}

void test_kqueue_socket_hup()
{
	ffkq kq = ffkq_create();
	ffsock l, lc, c;
	ffkq_task task = {};
	ffkq_event ev;
	ffkq_time tm;
	ffkq_time_set(&tm, 1000);

	// listen
	x_sys(FFSOCK_NULL != (l = ffsock_create_tcp(AF_INET, 0)));
	ffsockaddr a = {};
	char ip[] = {127,0,0,1};
	ffsockaddr_set_ipv4(&a, ip, 64000);
	x_sys(0 == ffsock_bind(l, &a));
	x_sys(0 == ffsock_listen(l, SOMAXCONN));

	// connect
	x_sys(FFSOCK_NULL != (c = ffsock_create_tcp(AF_INET, FFSOCK_NONBLOCK)));
	x_sys(0 == ffkq_attach_socket(kq, c, &c, FFKQ_READWRITE));
	int r = ffsock_connect_async(c, &a, &task);
	x_sys(r == 0 || fferr_last() == FFSOCK_EINPROGRESS);

	x_sys(FFSOCK_NULL != (lc = ffsock_accept(l, &a, 0)));

	if (r != 0) {
		x_sys(1 == ffkq_wait(kq, &ev, 1, tm));
		x(ffkq_event_data(&ev) == &c);
		ffkq_task_event_assign(&task, &ev);
	}

	x_sys(0 == ffsock_connect_async(c, &a, &task));

	char cbuf[1000] = {};
	r = ffsock_recv_async(c, cbuf, sizeof(cbuf), &task);
	x_sys(r < 0 && fferr_last() == FFSOCK_EINPROGRESS);

	// client -> server
	x_sys(6 == ffsock_send(c, "cldata", 6, 0));

	// server -> client
	x_sys(6 == ffsock_send(lc, "svdata", 6, 0));
	ffsock_fin(lc);
#ifdef FF_UNIX
	usleep(10*1000);
#endif
	ffsock_close(lc);

	// client <- server
	x_sys(1 == ffkq_wait(kq, &ev, 1, tm));
	x(ffkq_event_data(&ev) == &c);
#ifdef FF_LINUX
	x(ev.events & EPOLLERR);
	x(ev.events & EPOLLHUP);
#elif defined FF_BSD
	x(ev.flags & EV_EOF);
#endif

	x_sys(6 == ffsock_recv_async(c, cbuf, sizeof(cbuf), &task));
	x_sys(0 == ffsock_recv_async(c, cbuf, sizeof(cbuf), &task));

	/*
	SERVER                       Linux:
	[FIN,] CLOSE                 [recv()=6,] recv()=0
	unread data, FIN             recv()=0
	unread data, FIN, CLOSE      EPOLLERR|EPOLLHUP, [recv()=6,] recv()=0
	unread data, CLOSE           EPOLLERR|EPOLLHUP, [recv()=6,] recv()=-1 errno=ECONNRESET

	                             Windows:
	unread data, CLOSE           recv()=-1 errno=ERROR_NETNAME_DELETED
	unread data, CLOSE           unread data, recv()=-1 errno=WSAECONNRESET
	*/

	ffsock_close(l);
	ffsock_close(c);
	ffkq_close(kq);
}

void test_kqueue_socket_sendv()
{
	ffkq kq = ffkq_create();
	ffsock l, lc, c;
	ffkq_task task = {};
	ffkq_event ev;
	ffkq_time tm;
	ffkq_time_set(&tm, 1000);

	// listen
	x_sys(FFSOCK_NULL != (l = ffsock_create_tcp(AF_INET, 0)));
	ffsockaddr a = {};
	char ip[] = {127,0,0,1};
	ffsockaddr_set_ipv4(&a, ip, 64000);
	x_sys(0 == ffsock_bind(l, &a));
	x_sys(0 == ffsock_listen(l, SOMAXCONN));

	// connect
	x_sys(FFSOCK_NULL != (c = ffsock_create_tcp(AF_INET, FFSOCK_NONBLOCK)));
	x_sys(!ffsock_setopt(c, SOL_SOCKET, SO_SNDBUF, 128*1024));
	x_sys(0 == ffkq_attach_socket(kq, c, &c, FFKQ_READWRITE));
	int r = ffsock_connect_async(c, &a, &task);
	x_sys(r == 0 || fferr_last() == FFSOCK_EINPROGRESS);

	x_sys(FFSOCK_NULL != (lc = ffsock_accept(l, &a, 0)));

	if (r != 0) {
		x_sys(1 == ffkq_wait(kq, &ev, 1, tm));
		x(ffkq_event_data(&ev) == &c);
		ffkq_task_event_assign(&task, &ev);
	}

	x_sys(0 == ffsock_connect_async(c, &a, &task));

	// client -> server
	// send until the system buffer is full
	int n = 0;
	char cbuf[64000];
	ffiovec iov[3] = {};
	ffiovec_set(&iov[1], "cldata", 6);
	ffiovec_set(&iov[2], cbuf, sizeof(cbuf));
	for (;;) {
		r = ffsock_sendv_async(c, iov, 3, &task);
		if (r < 0 && fferr_last() == FFSOCK_EINPROGRESS)
			break;
		x_sys(r > 0);
		n += r;
	}

	// server <- client
	// receive until the system buffer is empty
	char lbuf[64000];
	r = ffsock_recv(lc, lbuf, sizeof(lbuf), 0);
	x_sys(r >= 6);
	x(!memcmp(lbuf, "cldata", 6));
	n -= r;
	while (n > 0) {
		r = ffsock_recv(lc, lbuf, sizeof(lbuf), 0);
		x_sys(r > 0);
		n -= r;
	}

	x_sys(1 == ffkq_wait(kq, &ev, 1, tm));
	x(ffkq_event_data(&ev) == &c);

	// client -> server
	r = ffsock_sendv_async(c, iov, 3, &task);
	x_sys(r > 0);

	ffsock_close(l);
	ffsock_close(lc);
	ffsock_close(c);
	ffkq_close(kq);
}

void test_kqueue_socket_recvfrom()
{
	ffkq kq = ffkq_create();
	ffsock l, c;
	ffkq_task task = {};
	ffkq_event ev;
	ffkq_time tm;
	ffkq_time_set(&tm, 1000);

	// server
	x_sys(FFSOCK_NULL != (l = ffsock_create_udp(AF_INET, FFSOCK_NONBLOCK)));
	x_sys(0 == ffkq_attach_socket(kq, l, &l, FFKQ_READ));
	ffsockaddr a = {};
	char ip[] = {127,0,0,1};
	ffsockaddr_set_ipv4(&a, ip, 64000);
	x_sys(0 == ffsock_bind(l, &a));

	// client
	x_sys(FFSOCK_NULL != (c = ffsock_create_udp(AF_INET, FFSOCK_NONBLOCK)));
	ffsockaddr ca = {};
	ffsockaddr_set_ipv4(&ca, ip, 64001);
	x_sys(0 == ffsock_bind(c, &ca));

	char lbuf[16];
	int r;
	r = ffsock_recvfrom_async(l, lbuf, sizeof(lbuf), &ca, &task);
	x_sys(r < 0 && fferr_last() == FFSOCK_EINPROGRESS);

	// client -> server
	x_sys(6 == ffsock_sendto(c, "cldata", 6, 0, &a));
	x_sys(6 == ffsock_sendto(c, "CLDATA", 6, 0, &a));

	x_sys(1 == ffkq_wait(kq, &ev, 1, tm));
	x(ffkq_event_data(&ev) == &l);

	// server <- client
	r = ffsock_recvfrom_async(l, lbuf, sizeof(lbuf), &ca, &task);
	x_sys(r == 6);
	x(!memcmp(lbuf, "cldata", 6));
	ffuint port = 0;
	ffsockaddr_ip_port(&ca, &port);
	x(port == 64001);

	// server <- client
	r = ffsock_recvfrom_async(l, lbuf, sizeof(lbuf), &ca, &task);
	x_sys(r == 6);
	x(!memcmp(lbuf, "CLDATA", 6));
	ffsockaddr_ip_port(&ca, &port);
	x(port == 64001);
	// server -> client
	x_sys(6 == ffsock_sendto(l, "svdata", 6, 0, &ca));

#ifdef FF_WIN
	// server -> client
	// server <- "port unreachable" error
	ffsockaddr bad_addr;
	ffsockaddr_set_ipv4(&bad_addr, ip, 64002);
	x_sys(6 == ffsock_sendto(l, "svdata", 6, 0, &bad_addr));
	r = ffsock_recvfrom_async(l, lbuf, sizeof(lbuf), &ca, &task);
	x_sys(r < 0 && fferr_last() == WSAECONNRESET);
#endif

	r = ffsock_recvfrom_async(l, lbuf, sizeof(lbuf), &ca, &task);
	x_sys(r < 0 && fferr_last() == FFSOCK_EINPROGRESS);

	x_sys(6 == ffsock_sendto(c, "cldata", 6, 0, &a));

	x_sys(1 == ffkq_wait(kq, &ev, 1, tm));
	x(ffkq_event_data(&ev) == &l);

	// server <- client
	r = ffsock_recvfrom_async(l, lbuf, sizeof(lbuf), &ca, &task);
	x_sys(r == 6);
	x(!memcmp(lbuf, "cldata", 6));
	ffsockaddr_ip_port(&ca, &port);
	x(port == 64001);

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
	test_kqueue_socket_io();
	test_kqueue_socket_hup();
	test_kqueue_socket_sendv();
	test_kqueue_socket_recvfrom();
}

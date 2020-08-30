/** ffos: socket.h tester
2020, Simon Zolin
*/

#include <FFOS/mem.h>
#include <FFOS/socket.h>
#include <FFOS/thread.h>
#include <FFOS/test.h>

int thread_socket_udp_server(void *param)
{
	ffsock sk = *(ffsock*)param;

	ffsockaddr addr;
	char buf[100];
	ffssize n = ffsock_recvfrom(sk, buf, sizeof(buf), 0, &addr);
	xint_sys(5, n);
	x(!ffmem_cmp(buf, "hello", 5));

	ffuint port = 0;
	ffslice ips = ffsockaddr_ip_port(&addr, &port);
	x(4 == ips.len);
	ffbyte *ip = (void*)ips.ptr;
	fflog("accepted connection from %u.%u.%u.%u port %d"
		, ip[0], ip[1], ip[2], ip[3]
		, port);

	return 0;
}

void test_socket_udp()
{
	ffsock l = ffsock_create_udp(AF_INET, 0);
	x_sys(l != FFSOCK_NULL);

	ffsockaddr addr = {};
	ffsockaddr_set_ipv4(&addr, NULL, 0);
	x_sys(0 == ffsock_bind(l, &addr));

	x_sys(0 == ffsock_localaddr(l, &addr));
	ffuint port = 0;
	ffslice ips = ffsockaddr_ip_port(&addr, &port);
	x(4 == ips.len);
	ffbyte *ip = (void*)ips.ptr;
	fflog("listening on %u.%u.%u.%u port %d"
		, ip[0], ip[1], ip[2], ip[3]
		, port);

	ffthread th = ffthread_create(thread_socket_udp_server, &l, 0);

	ffsock c = ffsock_create_udp(AF_INET, 0);
	x_sys(c != FFSOCK_NULL);
	ffsockaddr_set_ipv4(&addr, "\x7f\x00\x00\x01", port);
	ffssize n = ffsock_sendto(c, "hello", 5, 0, &addr);
	xint_sys(5, n);

	ffthread_join(th, -1, NULL);
	ffsock_close(l);
	ffsock_close(c);
}

int thread_socket_tcp_server(void *param)
{
	ffsock sk = *(ffsock*)param;

	ffsockaddr addr;
	ffsock c = ffsock_accept(sk, &addr, 0);
	x_sys(c != FFSOCK_NULL);

	ffuint port = 0;
	ffslice ips = ffsockaddr_ip_port(&addr, &port);
	x(16 == ips.len);
	ffbyte *ip = (void*)ips.ptr;
	fflog("accepted connection from %xu.%xu.%xu.%xu.%xu.%xu.%xu.%xu.%xu.%xu.%xu.%xu.%xu.%xu.%xu.%xu port %d"
		, ip[0], ip[1], ip[2], ip[3], ip[4], ip[5], ip[6], ip[7], ip[8], ip[9], ip[10], ip[11], ip[12], ip[13], ip[14], ip[15]
		, port);

	char buf[100];
	ffssize n = ffsock_recv(c, buf, sizeof(buf), 0);
	x_sys(n == 5);
	x(!ffmem_cmp(buf, "hello", 5));

	ffiovec iov[2];
	ffiovec_set(&iov[0], "header ", 7);
	ffiovec_set(&iov[1], "body", 4);
	n = ffsock_sendv(c, iov, 2);
	xint_sys(7+4, n);

	x_sys(0 == ffsock_fin(c));
	ffsock_close(c);
	return 0;
}

void test_socket_tcp()
{
	ffsock l = ffsock_create_tcp(AF_INET6, 0);
	x_sys(l != FFSOCK_NULL);
	x_sys(0 == ffsock_setopt(l, IPPROTO_IPV6, IPV6_V6ONLY, 0));

	x_sys(0 == ffsock_nonblock(l, 0));

	int recvbuf;
	x(0 == ffsock_getopt(l, SOL_SOCKET, SO_RCVBUF, &recvbuf));
	fflog("recvbuf: %d", recvbuf);

	x_sys(0 == ffsock_setopt(l, IPPROTO_TCP, TCP_NODELAY, 1));

	int r = ffsock_deferaccept(l, 1);
	fflog("ffsock_deferaccept(): %d", r);

	ffsockaddr addr = {};
	ffsockaddr_set_ipv6(&addr, NULL, 0);
	x_sys(0 == ffsock_bind(l, &addr));
	x_sys(0 == ffsock_listen(l, SOMAXCONN));

	x_sys(0 == ffsock_localaddr(l, &addr));
	ffuint port = 0;
	ffslice ips = ffsockaddr_ip_port(&addr, &port);
	x(16 == ips.len);
	ffbyte *ip = (void*)ips.ptr;
	fflog("listening on %xu.%xu.%xu.%xu.%xu.%xu.%xu.%xu.%xu.%xu.%xu.%xu.%xu.%xu.%xu.%xu port %d"
		, ip[0], ip[1], ip[2], ip[3], ip[4], ip[5], ip[6], ip[7], ip[8], ip[9], ip[10], ip[11], ip[12], ip[13], ip[14], ip[15]
		, port);

	ffthread th = ffthread_create(thread_socket_tcp_server, &l, 0);

	ffsock c = ffsock_create_tcp(AF_INET6, 0);
	x_sys(c != FFSOCK_NULL);
	// ::1
	ffsockaddr_set_ipv6(&addr, "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01", port);
	x_sys(0 == ffsock_connect(c, &addr));
	ffssize n = ffsock_send(c, "hello", 5, 0);
	xint_sys(5, n);
	char buf[100];
	n = ffsock_recv(c, buf, sizeof(buf), 0);
	xint_sys(7+4, n);
	x(!ffmem_cmp(buf, "header body", 7+4));

	ffthread_join(th, -1, NULL);
	ffsock_close(l);
	ffsock_close(c);
}

void test_socket()
{
	x_sys(0 == ffsock_init(FFSOCK_INIT_SIGPIPE | FFSOCK_INIT_WSA | FFSOCK_INIT_WSAFUNCS));
	test_socket_udp();
	test_socket_tcp();
}

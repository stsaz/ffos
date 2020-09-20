/* ffos: echo server
2020, Simon Zolin */

#include <FFOS/queue.h>
#include <FFOS/socket.h>
#include <FFOS/process.h>
#include <FFOS/ffos-extern.h>

struct ctx {
	ffkq kq;
	ffsock l;
};
struct ctx *g;


#define die(ex)  echo_die(ex, __LINE__)

void echo_die(int ex, int line)
{
	if (!ex)
		return;
	fflog("error on line %L errno=%d %s"
		, line, fferr_last(), fferr_strptr(fferr_last()));
	ffps_exit(1);
}


typedef void (*handler_t)(void *param);

struct conn {
	ffsock sk;
	handler_t handler;
};

void echo_accept(void *param);

void conn_read(void *param)
{
	struct conn *c = param;

	char buf[1024];
	for (;;) {
		ffssize r = ffsock_recv(c->sk, buf, sizeof(buf), 0);
		if (r < 0) {
			if (fferr_again(fferr_last()))
				return;
			die(1);
		} else if (r == 0) {
			fflog("%p: peer closed connection", c);
			ffsock_close(c->sk);
			ffmem_free(c);

			echo_accept(NULL);
			return;
		}
		fflog("%p: received %L bytes", c, r);

		r = ffsock_send(c->sk, buf, r, 0);
		if (r < 0)
			die(1);
		fflog("%p: sent %L bytes", c, r);
	}
}

int echo_accept1()
{
	ffsockaddr addr;
	ffsock csk = ffsock_accept(g->l, &addr, FFSOCK_NONBLOCK);
	if (csk == FFSOCK_NULL) {
		if (fferr_again(fferr_last()))
			return -1;
		die(1);
	}

	struct conn *c = ffmem_new(struct conn);
	c->sk = csk;
	c->handler = conn_read;

	int port;
	ffslice ip = ffsockaddr_ip_port(&addr, &port);
	const ffbyte *ipb = (void*)ip.ptr;
	fflog("%p: accepted connection from %u.%u.%u.%u:%u"
		, c, ipb[0], ipb[1], ipb[2], ipb[3]
		, port);

	die(0 != ffkq_attach_socket(g->kq, csk, c, FFKQ_READWRITE));
	conn_read(c);
	return 0;
}

void echo_accept(void *param)
{
	for (;;) {
		if (0 != echo_accept1());
			break;
	}
}

int main(int argc, char **argv)
{
	g = ffmem_new(struct ctx);
	g->kq = FFKQ_NULL;
	g->l = FFSOCK_NULL;
	int port = 0;

	if (argc == 2) {
		ffstr s = FFSTR_INIT(argv[1]);
		die(!ffstr_to_uint32(&s, &port));
	}

	die(FFKQ_NULL == (g->kq = ffkq_create()));

// listen
	die(FFSOCK_NULL == (g->l = ffsock_create_tcp(AF_INET, FFSOCK_NONBLOCK)));
	struct conn *c = ffmem_new(struct conn);
	c->handler = echo_accept;
	die(0 != ffkq_attach_socket(g->kq, g->l, c, FFKQ_READ));
	ffsockaddr addr;
	ffsockaddr_set_ipv4(&addr, NULL, port);
	die(0 != ffsock_bind(g->l, &addr));
	die(0 != ffsock_listen(g->l, SOMAXCONN));
	die(0 != ffsock_localaddr(g->l, &addr));
	ffsockaddr_ip_port(&addr, &port);
	fflog("listening on port %d", port);

// process
	ffkq_time t;
	ffkq_time_set(&t, -1);
	for (;;) {
		ffkq_event ev;
		int r = ffkq_wait(g->kq, &ev, 1, t);
		die(r < 0);

		struct conn *c = ffkq_event_data(&ev);
		c->handler(c);
	}

	ffmem_free(c);
	ffsock_close(g->l);
	ffkq_close(g->kq);
	return 0;
}

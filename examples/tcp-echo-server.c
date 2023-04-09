/* ffos: echo server
Receives TCP data from a client and sends it back.
2020, Simon Zolin */

/* Usage:
./echo-server [LISTEN_PORT]
nc IP LISTEN_PORT
*/

#include <FFOS/queue.h>
#include <FFOS/socket.h>
#include <FFOS/process.h>
#include <FFOS/std.h>
#include <FFOS/ffos-extern.h>

#define DBG(fmt, ...)  fflog("DBG : " fmt, ##__VA_ARGS__)
#define ERR(fmt, ...)  fflog("ERR : " fmt, ##__VA_ARGS__)
#define DIE(ex)  ecs_die(ex, __FUNCTION__, __FILE__, __LINE__)

void ecs_die(int ex, const char *func, const char *file, int line)
{
	if (!ex)
		return;
	int e = fferr_last();
	const char *err = fferr_strptr(e);
	ERR("fatal error in %s(), %s:%d: (%d) %s"
		, func, file, line, e, err);
	ffps_exit(1);
}


struct ecs_conf {
	uint listen_port;
};
struct ecs_conf *conf;

typedef void (*handler_t)(void *param);

struct ecs_task {
	handler_t handler;
};

struct ecs_srv {
	struct ecs_task task;
	ffkq kq;
	ffsock lsk;
};
struct ecs_srv *srv;


struct ecs_conn {
	struct ecs_task task;
	ffsock sk;
	char id[4*4];
};

void ecs_conn_send(struct ecs_conn *c, ffstr data)
{
	ffssize r = ffsock_send(c->sk, data.ptr, data.len, 0);
	if (r < 0)
		DIE(1);
	DBG("%p: sent %L bytes", c, data.len);
}

void ecs_conn_recv(void *param)
{
	struct ecs_conn *c = param;

	char buf[1024];
	for (;;) {
		ffssize r = ffsock_recv(c->sk, buf, sizeof(buf), 0);
		if (r < 0) {
			if (fferr_again(fferr_last()))
				return;
			DIE(1);
		} else if (r == 0) {
			DBG("%p: peer closed connection", c);
			ffsock_close(c->sk);
			ffmem_free(c);
			return;
		}
		DBG("%p: received %L bytes", c, r);

		ffstr data = FFSTR_INITN(buf, r);
		ecs_conn_send(c, data);
	}
}

int ecs_accept1()
{
	ffsockaddr addr;
	ffsock csk = ffsock_accept(srv->lsk, &addr, FFSOCK_NONBLOCK);
	if (csk == FFSOCK_NULL) {
		if (fferr_again(fferr_last()))
			return -1;
		DIE(1);
	}

	struct ecs_conn *c = ffmem_new(struct ecs_conn);
	c->sk = csk;
	c->task.handler = ecs_conn_recv;

	int port;
	ffslice ip = ffsockaddr_ip_port(&addr, &port);
	const ffbyte *ipb = (void*)ip.ptr;
	ffs_format(c->id, sizeof(c->id), "%u.%u.%u.%u%Z"
		, ipb[0], ipb[1], ipb[2], ipb[3]);
	DBG("%p: accepted connection from %s:%u"
		, c, c->id, port);

	DIE(0 != ffkq_attach_socket(srv->kq, csk, &c->task, FFKQ_READWRITE));
	ecs_conn_recv(c);
	return 0;
}

void ecs_accept(void *param)
{
	for (;;) {
		if (0 != ecs_accept1());
			break;
	}
}

void ecs_listen()
{
	DIE(FFSOCK_NULL == (srv->lsk = ffsock_create_tcp(AF_INET, FFSOCK_NONBLOCK)));

	srv->task.handler = ecs_accept;
	DIE(0 != ffkq_attach_socket(srv->kq, srv->lsk, &srv->task, FFKQ_READ));

	ffsockaddr addr;
	ffsockaddr_set_ipv4(&addr, NULL, conf->listen_port);
	DIE(0 != ffsock_bind(srv->lsk, &addr));
	DIE(0 != ffsock_listen(srv->lsk, SOMAXCONN));
	DIE(0 != ffsock_localaddr(srv->lsk, &addr));
	ffsockaddr_ip_port(&addr, &conf->listen_port);
	DBG("listening on port %d", conf->listen_port);
}

void ecs_run()
{
	ffkq_time t;
	ffkq_time_set(&t, -1);
	for (;;) {
		ffkq_event ev;
		int r = ffkq_wait(srv->kq, &ev, 1, t);
		DIE(r < 0);

		struct ecs_task *t = ffkq_event_data(&ev);
		t->handler(t);
	}
}

void ecs_init()
{
	srv->kq = FFKQ_NULL;
	srv->lsk = FFSOCK_NULL;
	DIE(FFKQ_NULL == (srv->kq = ffkq_create()));
}

void ecs_destroy()
{
	ffsock_close(srv->lsk);
	ffkq_close(srv->kq);
}

int main(int argc, char **argv)
{
	conf = ffmem_new(struct ecs_conf);

	srv = ffmem_new(struct ecs_srv);
	ecs_init();

	if (argc == 2) {
		ffstr s = FFSTR_INIT(argv[1]);
		DIE(!ffstr_to_uint32(&s, &conf->listen_port));
	}

	ecs_listen();
	ecs_run();
	ecs_destroy();
	return 0;
}

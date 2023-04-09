/* ffos: TCP message routing server
Receives TCP data from a client and sends it to all other clients.
2023, Simon Zolin */

/* Example diagram:
Client1 <->       <-> Client2
           MRouter
Client3 <->       <-> Client4
*/

/* Usage:
./tcp-msg-router [LISTEN_PORT]
nc IP LISTEN_PORT
nc IP LISTEN_PORT
*/

#include <FFOS/queue.h>
#include <FFOS/socket.h>
#include <FFOS/process.h>
#include <FFOS/std.h>
#include <FFOS/ffos-extern.h>
#include <ffbase/vector.h>


#define DBG(fmt, ...)  fflog("DBG : " fmt, ##__VA_ARGS__)
#define WARN(fmt, ...)  fflog("WARN: " fmt, ##__VA_ARGS__)
#define ERR(fmt, ...)  fflog("ERR : " fmt, ##__VA_ARGS__)
#define DIE(ex)  mrt_die(ex, __FUNCTION__, __FILE__, __LINE__)

void mrt_die(int ex, const char *func, const char *file, int line)
{
	if (!ex)
		return;
	int e = fferr_last();
	const char *err = fferr_strptr(e);
	ERR("fatal error in %s(), %s:%d: (%d) %s"
		, func, file, line, e, err);
	ffps_exit(1);
}


struct mrt_conf {
	uint lport;
};
struct mrt_conf *conf;

typedef void (*handler_t)(void *param);

struct mrt_task {
	handler_t handler;
};

struct mrt_srv {
	struct mrt_task task;
	ffkq kq;
	ffsock lsk;
	ffvec conns; // struct mrt_conn*[]
};
struct mrt_srv *srv;

void mrt_srv_accept(void *param);


struct mrt_conn {
	struct mrt_task task;
	ffsock sk;
	char id[4*4];
};

void mrt_srv_unlink_conn(void *c)
{
	void **it;
	FFSLICE_WALK(&srv->conns, it) {
		if (*it == c) {
			ffslice_rmswapT((ffslice*)&srv->conns, it - (void**)srv->conns.ptr, 1, void*);
			break;
		}
	}
}

void mrt_conn_free(struct mrt_conn *c)
{
	mrt_srv_unlink_conn(c);
	ffsock_close(c->sk);
	ffmem_free(c);
}

void mrt_conn_send(struct mrt_conn *c, ffstr data)
{
	ffssize r = ffsock_send(c->sk, data.ptr, data.len, 0);
	if (r < 0 && fferr_again(fferr_last())) {
		WARN("%s: client doesn't accept more data", c->id);
		return;
	}

	if (r < 0)
		DIE(1);
	DBG("%s: sent %L bytes", c->id, r);
}

void mrt_srv_send(ffstr data)
{
	struct mrt_conn **it;
	FFSLICE_WALK(&srv->conns, it) {
		struct mrt_conn *c = *it;
		mrt_conn_send(c, data);
	}
}

void mrt_conn_recv(void *param)
{
	struct mrt_conn *c = param;

	char buf[1024];
	for (;;) {
		ffssize r = ffsock_recv(c->sk, buf, sizeof(buf), 0);
		if (r < 0) {
			if (fferr_again(fferr_last()))
				return;
			DIE(1);
		} else if (r == 0) {
			DBG("%s: peer closed connection", c->id);
			mrt_conn_free(c);
			return;
		}
		DBG("%s: received %L bytes", c->id, r);

		ffstr data = FFSTR_INITN(buf, r);
		mrt_srv_send(data);
	}
}


int mrt_srv_accept1()
{
	ffsockaddr addr;
	ffsock csk = ffsock_accept(srv->lsk, &addr, FFSOCK_NONBLOCK);
	if (csk == FFSOCK_NULL) {
		if (fferr_again(fferr_last()))
			return -1;
		DIE(1);
	}

	struct mrt_conn *c = ffmem_new(struct mrt_conn);
	c->sk = csk;
	c->task.handler = mrt_conn_recv;
	*ffvec_pushT(&srv->conns, struct mrt_conn*) = c;

	int port;
	ffslice ip = ffsockaddr_ip_port(&addr, &port);
	const ffbyte *ipb = (void*)ip.ptr;
	ffs_format(c->id, sizeof(c->id), "%u.%u.%u.%u%Z"
		, ipb[0], ipb[1], ipb[2], ipb[3]);
	DBG("%p: accepted connection from %s:%u"
		, c, c->id, port);

	DIE(0 != ffkq_attach_socket(srv->kq, csk, &c->task, FFKQ_READWRITE));
	mrt_conn_recv(c);
	return 0;
}

void mrt_srv_accept(void *param)
{
	for (;;) {
		if (0 != mrt_srv_accept1());
			break;
	}
}

void mrt_srv_listen()
{
	DIE(FFSOCK_NULL == (srv->lsk = ffsock_create_tcp(AF_INET, FFSOCK_NONBLOCK)));

	srv->task.handler = mrt_srv_accept;
	DIE(0 != ffkq_attach_socket(srv->kq, srv->lsk, &srv->task, FFKQ_READ));

	ffsockaddr addr;
	ffsockaddr_set_ipv4(&addr, NULL, conf->lport);
	DIE(0 != ffsock_bind(srv->lsk, &addr));
	DIE(0 != ffsock_listen(srv->lsk, SOMAXCONN));
	DIE(0 != ffsock_localaddr(srv->lsk, &addr));
	ffsockaddr_ip_port(&addr, &conf->lport);
	DBG("listening on port %d", conf->lport);
}

void mrt_run()
{
	ffkq_time t;
	ffkq_time_set(&t, -1);
	for (;;) {
		ffkq_event ev;
		int r = ffkq_wait(srv->kq, &ev, 1, t);
		DIE(r < 0);

		struct mrt_task *t = ffkq_event_data(&ev);
		t->handler(t);
	}
}

void mrt_destroy()
{
	ffsock_close(srv->lsk);
	ffkq_close(srv->kq);
}

int main(int argc, char **argv)
{
	conf = ffmem_new(struct mrt_conf);

	srv = ffmem_new(struct mrt_srv);
	srv->kq = FFKQ_NULL;
	srv->lsk = FFSOCK_NULL;

	if (argc == 2) {
		ffstr s = FFSTR_INIT(argv[1]);
		DIE(!ffstr_to_uint32(&s, &conf->lport));
	}

	DIE(FFKQ_NULL == (srv->kq = ffkq_create()));

	mrt_srv_listen();
	mrt_run();
	mrt_destroy();
	ffmem_free(srv);
	return 0;
}

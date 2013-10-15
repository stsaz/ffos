#include <FFOS/socket.h>
#include <FFOS/string.h>
#include <FFOS/thread.h>
#include <FFOS/error.h>
#include <FFOS/test.h>
#include <stdio.h>

#define x FFTEST_BOOL

#define HOST TEXT("localhost")
#define PORT TEXT("8080")
#define REQ "GET / HTTP/1.0" FFCRLF FFCRLF
#define RESP_HDR "HTTP/1.0 200 OK" FFCRLF FFCRLF
#define RESP_BODY "this is body" FFCRLF
#define RESP RESP_HDR RESP_BODY

typedef struct {
	const ffsyschar *host;
	const ffsyschar *port;
	const char *req;
	const char *respHdr;
	const char *respBody;
} servData;

#define CLIID "client: "

static int test_sktsyncclient(const servData *sd)
{
	ffskt sk = FF_BADSKT;
	char buf[4096];
	ssize_t r;
	ffaddrinfo *a;
	ffaddrinfo *addrs;
	int e;
	size_t reqsz = strlen(sd->req);

	FFTEST_FUNC;

	e = ffaddr_info(&addrs, sd->host, sd->port, 0, SOCK_STREAM, IPPROTO_TCP);
	x(e == 0);

	for (a = addrs;  a != NULL;  a = a->ai_next) {
		sk = ffskt_create(a->ai_family, a->ai_socktype, a->ai_protocol);
		if (sk == FF_BADSKT)
			break;
		printf("%s", CLIID "connecting..." FF_NEWLN);
		if (0 == ffskt_connect(sk, a->ai_addr, a->ai_addrlen))
			break;
		ffskt_close(sk);
		sk = FF_BADSKT;
	}
	ffaddr_free(addrs);
	if (!x(sk != FF_BADSKT))
		return 1;

	x(0 == ffskt_nblock(sk, 1));
	x(-1 == ffskt_recv(sk, buf, sizeof(buf), 0) && fferr_again(fferr_last()));
	x(0 == ffskt_nblock(sk, 0));

	printf(CLIID "sending %d bytes..." FF_NEWLN, (int)reqsz);
	x(reqsz == ffskt_send(sk, sd->req, reqsz, 0));
	printf("%s", CLIID "sending FIN..." FF_NEWLN);
	x(0 == ffskt_fin(sk));

	for (;;) {
		r = ffskt_recv(sk, buf, sizeof(buf), 0);
		if (r > 0) {
			printf(CLIID "received %d bytes" FF_NEWLN, (int)r);
			continue;
		}
		x(r != -1);
		if (r == 0)
			printf("%s", CLIID "received FIN" FF_NEWLN);
		break;
	}

	x(0 == ffskt_close(sk));
	return 0;
}

#define SRVID "server: "

static int FFTHDCALL test_sktsyncserver(void *param)
{
	const servData *sd = param;
	ffskt sk
		, csk;
	ffaddrinfo *addrs;
	ffaddrinfo *a;
	ffaddr peer
		, local;
	char buf[1024];
	size_t ibuf = 0;

	FFTEST_FUNC;

	x(0 == ffaddr_info(&addrs, sd->host, sd->port, AI_PASSIVE, SOCK_STREAM, IPPROTO_TCP));
	a = addrs;

	sk = ffskt_create(a->ai_family, a->ai_socktype, a->ai_protocol);
	x(sk != FF_BADSKT);

	x(0 == ffskt_bind(sk, a->ai_addr, a->ai_addrlen));
	ffaddr_free(addrs);

	x(0 == ffskt_listen(sk, SOMAXCONN));
	printf(SRVID "listening..." FF_NEWLN);

	peer.len = sizeof(struct sockaddr_in6);
	csk = ffskt_accept(sk, &peer.a, &peer.len, 0);
	x(csk != FF_BADSKT);
	{
		int recvbuf;
		x(0 == ffskt_getopt(csk, SOL_SOCKET, SO_RCVBUF, &recvbuf));
		x(recvbuf != 0);
		x(0 == ffskt_setopt(csk, SOL_SOCKET, SO_RCVBUF, 1 * 1024));
	}
	x(0 == ffskt_local(csk, &local));

	{
		char peerAddr[FF_MAXIP6];
		char peerPort[NI_MAXSERV];
		char localAddr[FF_MAXIP6];
		char localPort[NI_MAXSERV];
		x(0 == ffaddr_name(&local.a, local.len, localAddr, sizeof(localAddr)
			, localPort, sizeof(localPort), NI_NUMERICHOST | NI_NUMERICSERV));
		x(0 == ffaddr_name(&peer.a, peer.len, peerAddr, sizeof(peerAddr)
			, peerPort, sizeof(peerPort), NI_NUMERICHOST | NI_NUMERICSERV));
		printf(SRVID "client %s:%s has connected to %s:%s" FF_NEWLN
			, peerAddr, peerPort, localAddr, localPort);
	}

	for (;;) {
		ssize_t r = ffskt_recv(csk, buf + ibuf, sizeof(buf) - ibuf, 0);
		if (r > 0) {
			printf(SRVID "received %d bytes" FF_NEWLN, (int)r);
			ibuf += r;
			if (0 == x(ibuf != sizeof(buf)))
				break;
			continue;
		}
		x(r != -1);
		if (r == 0)
			printf("%s", SRVID "received FIN" FF_NEWLN);
		break;
	}

	x(ibuf == strlen(sd->req) && !memcmp(buf, sd->req, ibuf));

	{
		size_t r;
		ffiovec iov[2];
		ffiov_set(&iov[0], sd->respHdr, strlen(sd->respHdr));
		ffiov_set(&iov[1], sd->respBody, strlen(sd->respBody));
		r = ffskt_sendv(csk, iov, 2);
		x(r == iov[0].iov_len + iov[1].iov_len);
		printf(SRVID "sent %d bytes" FF_NEWLN, (int)r);
	}

	x(0 == ffskt_fin(csk));
	x(0 == ffskt_close(csk));
	x(0 == ffskt_close(sk));
	return 0;
}

static int test_addr()
{
	ffaddr a;

	FFTEST_FUNC;

	ffaddr_init(&a, AF_INET, sizeof(struct sockaddr_in));
	x(ffaddr_family(&a) == AF_INET);
	ffip_setv4addr(&a.ip4, INADDR_LOOPBACK);
	x(a.ip4.sin_addr.s_addr == ffhton32(INADDR_LOOPBACK));
	ffip_setport(&a, 8080);
	x(ffip_port(&a) == 8080);

	ffaddr_init(&a, AF_INET6, sizeof(struct sockaddr_in6));
	x(ffaddr_family(&a) == AF_INET6);
	ffip_setv6addr(&a.ip6, &in6addr_loopback);
	x(ffip_v6equal(&a.ip6.sin6_addr, &in6addr_loopback));
	x(!ffip_v4mapped(&a.ip6.sin6_addr));

	ffip_setv4mapped(&a.ip6, INADDR_LOOPBACK);
	x(ffip_v4mapped(&a.ip6.sin6_addr));
	ffip_v4mapped_tov4(&a);
	x(a.ip4.sin_addr.s_addr == ffhton32(INADDR_LOOPBACK));
	x(ffaddr_family(&a) == AF_INET);

	return 0;
}

int test_skt()
{
	servData sd;
	ffthd th;

	FFTEST_FUNC;

	test_addr();

	x(0 == ffskt_init(FFSKT_WSA | FFSKT_WSAFUNCS));

	sd.host = HOST;
	sd.port = PORT;
	sd.req = REQ;
	sd.respHdr = RESP_HDR;
	sd.respBody = RESP_BODY;
	th = ffthd_create(&test_sktsyncserver, &sd, 0);
	x(th != 0);
	x(ETIMEDOUT == ffthd_join(th, 100, NULL));
	x(ETIMEDOUT == ffthd_join(th, 0, NULL));

	test_sktsyncclient(&sd);

	{
		int ecode;
		x(0 == ffthd_join(th, (uint)-1, &ecode));
		x(ecode == 0);
	}

	return 0;
}

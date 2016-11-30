/**
Copyright (c) 2014 Simon Zolin
*/

#include <sys/socket.h>


#define TCP_NOPUSH  TCP_CORK

FF_EXTN ffskt ffskt_accept(ffskt listenSk, struct sockaddr *a, socklen_t *addrlen, int flags);

/** Set defer-accept option on a listening TCP socket. */
#define ffskt_deferaccept(sk, enable) \
	ffskt_setopt(sk, IPPROTO_TCP, TCP_DEFER_ACCEPT, enable)

/** Headers and trailers for sendfile() function. */
struct sf_hdtr {
	ffiovec *headers;
	int hdr_cnt;
	ffiovec *trailers;
	int trl_cnt;
};

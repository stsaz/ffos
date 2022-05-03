/** ffos: ffkq_task declaration for socket.h, pipe.h and queue.h
2022, Simon Zolin
*/

#pragma once

#if defined FF_UNIX

/** Used by ffsock_connect_async() */
typedef struct ffkq_task {
	ffushort active;
	ffushort kev_flags; // epoll: EPOLLERR;  kqueue: EV_EOF
	ffuint kev_errno; // kqueue: error code for EV_EOF and EVFILT_READ or EVFILT_WRITE
} ffkq_task;
typedef ffkq_task ffkq_task_accept;

#else

/** Used by ffsock_connect_async(), ffsock_recv_async(), ffsock_send_async() */
typedef struct ffkq_task {
	OVERLAPPED overlapped;
	ffuint active;
	char buf[1];
} ffkq_task;

/** Used by ffsock_accept_async() */
typedef struct {
	OVERLAPPED overlapped;
	ffuint active;
	ffbyte local_peer_addrs[(sizeof(struct sockaddr_in6) + 16) * 2];
	SOCKET csock;
} ffkq_task_accept;

#endif

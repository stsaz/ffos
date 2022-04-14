/** ffos: detect CPU, OS and compiler; base types; heap memory functions
2020, Simon Zolin
*/

#ifndef _FFOS_BASE_H
#define _FFOS_BASE_H

#include <FFOS/detect-cpu.h>
#include <FFOS/detect-os.h>

#if defined FF_LINUX
	#ifndef _GNU_SOURCE
		#define _GNU_SOURCE
	#endif

	#ifndef _LARGEFILE64_SOURCE
		#define _LARGEFILE64_SOURCE 1
	#endif
#endif

#if defined FF_UNIX
	#include <stdlib.h>
	#include <unistd.h>
	#include <string.h>
	#include <errno.h>

	typedef int fffd;
	typedef char ffsyschar;

typedef struct ffkq_task {
	unsigned short active;
	unsigned short kev_flags; // epoll: EPOLLERR;  kqueue: EV_EOF
	unsigned int kev_errno; // kqueue: error code for EV_EOF and EVFILT_READ or EVFILT_WRITE
} ffkq_task;

#else // Windows:

	#define _WIN32_WINNT FF_WIN_APIVER
	#define NOMINMAX
	#define _CRT_SECURE_NO_WARNINGS
	#define OEMRESOURCE //gui
	#include <winsock2.h>
	#include <ws2tcpip.h>

	typedef HANDLE fffd;
	typedef wchar_t ffsyschar;

typedef struct ffkq_task {
	OVERLAPPED overlapped;
	unsigned active;
	unsigned char local_peer_addrs[(sizeof(struct sockaddr_in6) + 16) * 2];
	SOCKET csock;
} ffkq_task;

#endif

#include <ffbase/base.h>

#include <FFOS/detect-compiler.h>
#include <FFOS/compiler-gcc.h>

typedef signed char ffint8;
typedef int ffbool;

#endif

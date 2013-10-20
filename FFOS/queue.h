/**
Kernel queue.  kqueue, epoll, I/O completion ports.
Copyright (c) 2013 Simon Zolin
*/

#pragma once

#include <FFOS/types.h>

#if defined FF_BSD
	#include <FFOS/unix/kqu-bsd.h>
#elif defined FF_LINUX
	#include <FFOS/unix/kqu-epoll.h>
#elif defined FF_WIN
	#include <FFOS/win/kqu-iocp.h>
#endif

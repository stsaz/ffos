---------------
FAST-FORWARD (FF) LIBRARY
Unified Operating System interface.
---------------

FFOS library allows to write cross-platform code for these OS:
* Linux
* Windows
* FreeBSD
* macOS

The supported languages are C and C++.
The supported compilers: gcc (+MinGW), clang.

The resulting code is almost completely inlined by a compiler: the code is native, high-performance but still cross-platform.


--------
FEATURES
--------

* system types - `FFOS/types.h`
* files - `FFOS/file.h`
* pipes - `FFOS/file.h`
* directories - `FFOS/dir.h`
* native file-system paths - `FFOS/dir.h`
* timer, clock - `FFOS/timer.h`
* process - `FFOS/process.h`
* dynamically loaded libraries - `FFOS/process.h`
* sockets - `FFOS/socket.h`
* network address; IPv4/IPv6 address - `FFOS/socket.h`
* threads - `FFOS/thread.h`
* kernel queue - `FFOS/queue.h`
* asynchronous I/O - `FFOS/asyncio.h`
* UNIX signals - `FFOS/sig.h`
* heap memory - `FFOS/mem.h`
* native system string - `FFOS/string.h`
* date and time - `FFOS/time.h`
* system error codes - `FFOS/error.h`
* atomic integer functions - `FFOS/atomic.h`
* CPUID - `FFOS/cpuid.h`
* random number generator - `FFOS/random.h`


--------
LICENSE
--------

The code provided here is free for use in open-source and proprietary projects.

You may distribute, redistribute, modify the whole code or the parts of it, just keep the original copyright statement inside the files.

--------

Simon Zolin

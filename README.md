# ffos

FFOS library allows to write cross-platform code for these OS:
* Linux
* Windows
* FreeBSD
* macOS

The supported languages are C and C++.
The supported compilers: gcc (+MinGW), clang.

The resulting code is almost completely inlined by a compiler: the code is native, high-performance but still cross-platform.


## Requirements

* gcc or clang
* ffbase (base types & algorithms)


## FEATURES

I/O:
* standard I/O - `FFOS/std.h`
* files - `FFOS/file.h`
* file mapping - `FFOS/filemap.h`
* unnamed and named pipes - `FFOS/pipe.h`
* asynchronous I/O - `FFOS/asyncio.h`
* kernel queue - `FFOS/queue.h`

Process, thread, IPC:
* process - `FFOS/process.h`
* process environment - `FFOS/process.h`
* threads - `FFOS/thread.h`
* UNIX signals - `FFOS/signal.h`, `FFOS/sig.h`
* semaphores - `FFOS/semaphore.h`
* process/thread performance counters `ffps_perf` - `FFOS/perf.h`
* dynamically loaded libraries - `FFOS/dylib.h`
* backtrace - `FFOS/backtrace.h`

Network:
* sockets - `FFOS/socket.h`
* network address; IPv4/IPv6 address - `FFOS/socket.h`
* network configuration `ffnetconf` - `FFOS/netconf.h`

Misc:
* system types - `FFOS/types.h`
* preprocessor detection of OS, CPU, compiler - `FFOS/detect-*.h`
* directories - `FFOS/dir.h`
* native file-system paths - `FFOS/dir.h`
* timer, clock - `FFOS/timer.h`
* heap memory - `FFOS/mem.h`
* native system string - `FFOS/string.h`
* date and time - `FFOS/time.h`
* system error codes - `FFOS/error.h`
* atomic integer functions - `FFOS/atomic.h`
* CPUID - `FFOS/cpuid.h`
* random number generator - `FFOS/random.h`


## Build & Test

	git clone https://github.com/stsaz/ffbase
	git clone https://github.com/stsaz/ffos
	cd ffos
	make -j8 -Rr
	cd test
	./ffostest all

on FreeBSD:

	gmake -j8

on Linux for Windows:

	make -j8 OS=win CPREFIX=x86_64-w64-mingw32-


## License

This code is absolutely free.

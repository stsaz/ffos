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
* UNIX signals, execution exceptions - `FFOS/signal.h`
* semaphores - `FFOS/semaphore.h`
* process/thread performance counters `ffps_perf` - `FFOS/perf.h`
* dynamically loaded libraries - `FFOS/dylib.h`
* backtrace - `FFOS/backtrace.h`

Network:
* sockets - `FFOS/socket.h`
* network address; IPv4/IPv6 address - `FFOS/socket.h`
* network configuration `ffnetconf` - `FFOS/netconf.h`

Misc:
* detect CPU, OS and compiler; base types; heap memory functions - `FFOS/base.h`
* preprocessor detection of OS, CPU, compiler - `FFOS/detect-*.h`
* directories - `FFOS/dir.h`
* native file-system paths - `FFOS/path.h`
* timer, clock - `FFOS/timer.h`
* timer queue - `FFOS/timerqueue.h`
* native system string - `FFOS/string.h`
* date and time - `FFOS/time.h`
* system error codes - `FFOS/error.h`
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

	make -j8 OS=windows


## License

This code is absolutely free.

# ffos

FFOS library allows to write cross-platform code for these OS:
* Linux (+Android)
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

```
General:
  base.h        Detect CPU, OS and compiler; base types; heap memory functions
  ffos-extern.h Define external symbols

I/O, FS:
  std.h         Standard I/O
  file.h        Files
  filemap.h     File mapping
  pipe.h        Unnamed and named pipes
  queue.h       Kernel queue
  kcall.h       Kernel call queue (to call kernel functions asynchronously)
  dir.h         File-system directory functions
  dirscan.h     Scan directory for files
  path.h        Native file-system paths
  filemon.h     File system monitoring (Linux, Windows) (incompatible)
  volume.h      Volumes (Windows)

Process, thread, IPC:
  process.h     Process
  environ.h     Process environment
  thread.h      Threads
  signal.h      UNIX signals, CPU exceptions
  semaphore.h   Semaphores
  perf.h        Process/thread performance counters
  dylib.h       Dynamically loaded libraries
  backtrace.h   Backtrace

Network:
  socket.h      Sockets, network address
  netconf.h     Network configuration
  netlink.h     Linux netlink helper functions

Misc:
  time.h        Date and time
  timer.h       System timer
  timerqueue.h  Timer queue
  string.h      Native system string
  error.h       System error codes
  random.h      Random number generator
```

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

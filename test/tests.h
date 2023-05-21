/** ffos: the list of auto and manual tests */

#define FFOS_TESTS_AUTO(X) \
	X(backtrace) \
	X(dir) \
	X(dirscan) \
	X(dylib) \
	X(env) \
	X(error) \
	X(file) \
	X(filemap) \
	X(kcall) \
	X(kqueue) \
	X(mem) \
	X(path) \
	X(perf) \
	X(pipe) \
	X(process) \
	X(rand) \
	X(semaphore) \
	X(socket) \
	X(sysconf) \
	X(thread) \
	X(time) \
	X(timer) \
	X(timerqueue) \

#ifdef FF_WIN
#define FFOS_TESTS_AUTO_OS(X) \
	X(filemon) \
	X(winreg) \

#elif defined FF_LINUX
#define FFOS_TESTS_AUTO_OS(X) \
	X(filemon) \
	X(unixsignal) \

#else
#define FFOS_TESTS_AUTO_OS(X)
#endif

// X(atomic)
// X(fileaio)
// X(kqu)

#define FFOS_TESTS_MANUAL(X) \
	X(std) \
	X(std_event) \
	X(file_dosname) /*8.3 support may be disabled globally*/ \
	X(netconf) /*network may be disabled*/ \
	X(sig_abort) \
	X(sig_ctrlc) \
	X(sig_fpe) \
	X(sig_segv) \
	X(sig_stack) \
	X(vol_mount) /*need admin rights*/

#ifdef FF_LINUX
#define FFOS_TESTS_MANUAL_OS(X) \
	X(netlink)

#else
#define FFOS_TESTS_MANUAL_OS(X)
#endif

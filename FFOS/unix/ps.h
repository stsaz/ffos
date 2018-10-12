/**
Copyright (c) 2013 Simon Zolin
*/

#include <signal.h>
#include <dlfcn.h>


typedef int ffps;
enum { FFPS_INV = -1 };

/** Process fork.
Return child PID.  Return 0 in the child process.
Return -1 on error. */
#define ffps_fork  fork

/** Get process ID by process handle. */
#define ffps_id(h)  ((uint)(h))

/** Kill the process. */
#define ffps_kill(h)  kill(h, SIGKILL)

/** Send signal to a process. */
#define ffps_sig(h, sig)  kill(h, sig)

/** Close process handle. */
#define ffps_close(h)  (0)

/** Wait for the process. */
FF_EXTN int ffps_wait(ffps h, uint, int *exit_code);

/** Get the current process handle. */
#define ffps_curhdl  getpid

/** Get ID of the current process. */
#define ffps_curid  getpid

/** Exit the current process. */
#define ffps_exit  _exit


#ifdef FF_APPLE
#define FFDL_EXT  "dylib"
#else
#define FFDL_EXT  "so"
#endif

typedef void * ffdl;
typedef void * ffdl_proc;

enum FFDL_OPEN {
	/** Windows: load dependencies from the library's directory first.
	Filename parameter must be absolute and must not contain '/'. */
	FFDL_SELFDIR = 0,
};

/** Open library.
@flags: enum FFDL_OPEN
Return NULL on error. */
#define ffdl_open(filename, flags)  dlopen(filename, (flags) | RTLD_LAZY)

/** Get the function address from the library.
Return NULL on error. */
#define ffdl_addr(dl, name)  dlsym(dl, name)

/** Get last error message. */
#define ffdl_errstr()  dlerror()

/** Close the library. */
#define ffdl_close(dl)  dlclose(dl)


typedef void * ffsysconf;

/** Init sysconf structure. */
#define ffsc_init(sc) ((void)sc)

/** Get value from system config. */
#define ffsc_get(sc, name)  sysconf(name)

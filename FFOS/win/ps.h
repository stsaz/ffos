/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/file.h>


FF_EXTN int ffenv_update(void);


typedef HANDLE ffps;
#define FFPS_INV  INVALID_HANDLE_VALUE

/**
argv: quotes in arguments are not escaped.
env: not implemented. */
FF_EXTN ffps ffps_exec(const char *filename, const char **argv, const char **env);

#define ffps_id  GetProcessId

/** Send signal to a process.  Works only if the other process has called ffsig_ctl(). */
FF_EXTN int ffps_sig(int pid, int sig);

#define ffps_kill(h)  (0 == TerminateProcess(h, -9 /*-SIGKILL*/))

static FFINL int ffps_close(ffps h) {
	return 0 == CloseHandle(h);
}

FF_EXTN int ffps_wait(ffps h, uint timeout_ms, int *exit_code);

#define ffps_curhdl  GetCurrentProcess

#define ffps_curid  GetCurrentProcessId

#define ffps_exit  ExitProcess


#define FFDL_EXT  "dll"

typedef HMODULE ffdl;
typedef FARPROC ffdl_proc;

enum FFDL_OPEN {
	FFDL_SELFDIR = LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32,
};

/** Prepare for using ffdl_open() with flags, because some Windows versions may not support this (EINVAL).
Must be called once per module, but affects the whole process.
@path: path where .dll dependencies will be searched;  use NULL to restore to defaults. */
FF_EXTN int ffdl_init(const char *path);

FF_EXTN ffdl ffdl_openq(const ffsyschar *filename, uint flags);

FF_EXTN fffd ffdl_open(const char *filename, int flags);

#define ffdl_addr(dl, name)  GetProcAddress(dl, name)

#define ffdl_errstr()  fferr_strp(fferr_last())

static FFINL int ffdl_close(fffd h) {
	return 0 == FreeLibrary(h);
}


enum FFSC_I {
	_SC_PAGESIZE = 1
	, _SC_NPROCESSORS_ONLN
};

typedef SYSTEM_INFO ffsysconf;

static FFINL void ffsc_init(ffsysconf *sc) {
	GetNativeSystemInfo(sc);
}

static FFINL int ffsc_get(ffsysconf *sc, int name) {
	switch (name) {
	case _SC_PAGESIZE:
		return sc->dwAllocationGranularity;
	case _SC_NPROCESSORS_ONLN:
		return sc->dwNumberOfProcessors;
	}
	return 0;
}

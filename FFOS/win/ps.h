/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/file.h>


static FFINL size_t ffenv_expand(const ffsyschar *src, ffsyschar *dst, size_t cap) {
	size_t r = ExpandEnvironmentStrings(src, dst, FF_TOINT(cap));
	if (r > cap)
		return 0;
	return r;
}


/**
argv: quotes in arguments are not escaped.
env: not implemented. */
FF_EXTN fffd ffps_exec(const char *filename, const char **argv, const char **env);

#define ffps_id  GetProcessId

/** Send signal to a process.  Works only if the other process has called ffsig_ctl(). */
FF_EXTN int ffps_sig(int pid, int sig);

#define ffps_kill(h)  (0 == TerminateProcess(h, -9 /*-SIGKILL*/))

static FFINL int ffps_close(fffd h) {
	return 0 == CloseHandle(h);
}

FF_EXTN int ffps_wait(fffd h, uint timeout_ms, int *exit_code);

#define ffps_curhdl  GetCurrentProcess

#define ffps_curid  GetCurrentProcessId

FF_EXTN const char* ffps_filename(char *name, size_t cap, const char *argv0);

#define ffps_exit  ExitProcess


#define FFDL_EXT  "dll"

typedef HMODULE ffdl;
typedef FARPROC ffdl_proc;

#define ffdl_openq(filename, flags) \
	LoadLibraryEx(filename, NULL, (flags) | LOAD_WITH_ALTERED_SEARCH_PATH)

FF_EXTN fffd ffdl_open(const char *filename, int flags);

#define ffdl_addr  GetProcAddress

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

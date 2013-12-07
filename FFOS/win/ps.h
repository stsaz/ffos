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
FF_EXTN fffd ffps_exec(const ffsyschar *filename, const ffsyschar **argv, const ffsyschar **env);

#define ffps_id  GetProcessId

#define ffps_kill(h)  (0 == TerminateProcess(h, -1))

#define ffps_close(h)  (0 == CloseHandle(h))

FF_EXTN int ffps_wait(fffd h, uint timeout_ms, int *exit_code);

#define ffps_curhdl  GetCurrentProcess

#define ffps_curid  GetCurrentProcessId

#define ffps_exit  ExitProcess


#define FFDL_EXT  "dll"

typedef HMODULE ffdl;
typedef FARPROC ffdl_proc;

#define ffdl_open(filename, flags) \
	LoadLibraryEx(filename, NULL, (flags) | LOAD_WITH_ALTERED_SEARCH_PATH)

#define ffdl_addr  GetProcAddress

#define ffdl_close(h)  (0 == FreeLibrary(h))

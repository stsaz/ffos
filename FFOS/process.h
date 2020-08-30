/** Process, dynamic library, system config.
Copyright (c) 2013 Simon Zolin
*/

#pragma once

#include <FFOS/types.h>
#include <FFOS/time.h>


typedef ffbyte ffenv;
static inline int ffenv_init(ffenv *env, char **envptr){return 0;}
static inline void ffenv_destroy(ffenv *env){}

#ifdef FF_UNIX

/** Expand environment variables in a string.
src:
UNIX: "text $VAR text"
Windows: "text %VAR% text"
dst: expanded string, NULL-terminated
 NULL: allocate new buffer
Return 'dst' or a new allocated buffer;
 NULL on error */
static char* ffenv_expand(void *unused, char *dst, size_t cap, const char *src);

#include <FFOS/unix/ps.h>

#elif defined FF_WIN

#include <FFOS/win/ps.h>

FF_EXTN char* ffenv_expand(void *unused, char *dst, size_t cap, const char *src);

#endif

typedef struct ffps_execinfo {
	/** List of process arguments: ["process", "arg1", "arg2", NULL]
	 Windows: double-quotes in arguments are not escaped. */
	const char **argv;

	/** List of environment variables.
	 Windows: not implemented. */
	const char **env;

	/** Standard file descriptors. */
	fffd in, out, err;
} ffps_execinfo;

/** Create a new process.
Return FFPS_INV on error. */
FF_EXTN ffps ffps_exec2(const char *filename, ffps_execinfo *info);

static inline ffps ffps_exec(const char *filename, const char **argv, const char **env)
{
	ffps_execinfo info = {};
	info.argv = argv;
	info.env = env;
	info.in = info.out = info.err = FF_BADFD;
	return ffps_exec2(filename, &info);
}

/** Wait for the process to exit.
timeout_ms: Maximum time to wait
 UNIX: only 0 or !0 value is supported.
Return 0 on success.  Process handle is closed, so don't call ffps_close() in this case. */
FF_EXTN int ffps_wait(ffps h, uint timeout_ms, int *exit_code);

/** Get filename of the current process. */
FF_EXTN const char* ffps_filename(char *name, size_t cap, const char *argv0);

/** Create a copy of the current process in background.
Return child process descriptor (parent);  0 (child);  -1 on error. */
FF_EXTN ffps ffps_createself_bg(const char *arg);

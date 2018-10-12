/** Process, dynamic library, system config.
Copyright (c) 2013 Simon Zolin
*/

#pragma once

#include <FFOS/types.h>


typedef struct ffenv {
	size_t n;
	char **ptr;
} ffenv;

FF_EXTN int ffenv_init(ffenv *env, char **envptr);

FF_EXTN void ffenv_destroy(ffenv *env);

/** Expand environment variables in a string.
UNIX: "text $VAR text"
Windows: "text %VAR% text"
@dst: expanded string
Return 'dst' or a new allocated buffer (if dst == NULL). */
FF_EXTN char* ffenv_expand(ffenv *env, char *dst, size_t cap, const char *src);


#ifdef FF_UNIX
#include <FFOS/unix/ps.h>

#elif defined FF_WIN
#include <FFOS/win/ps.h>
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

/** Get filename of the current process. */
FF_EXTN const char* ffps_filename(char *name, size_t cap, const char *argv0);

/** Create a copy of the current process in background.
Return child process descriptor (parent);  0 (child);  -1 on error. */
FF_EXTN ffps ffps_createself_bg(const char *arg);


enum FFPS_PERF {
	FFPS_PERF_REALTIME = 1,
	FFPS_PERF_CPUTIME = 2, //summarized CPU time
	FFPS_PERF_SEPTIME = 4, //user and system CPU time
	FFPS_PERF_RUSAGE = FFPS_PERF_SEPTIME | 8, //user/systime, pagefaults, maxrss, in/outblock, v/ivctxsw
};

struct ffps_perf {
	fftime realtime;
	fftime cputime; //UNIX: higher precision than usertime/systime
	fftime usertime;
	fftime systime;
	uint pagefaults;
	uint maxrss; //in Kb
	uint inblock;
	uint outblock;
	uint vctxsw;
	uint ivctxsw;
};

/** Get information about the process.
@flags: enum FFPS_PERF */
FF_EXTN int ffps_perf(struct ffps_perf *inf, uint flags);

/** Get information about the thread.
Windows: only FFPS_PERF_CPUTIME and FFPS_PERF_SEPTIME are supported.
@flags: enum FFPS_PERF */
FF_EXTN int ffthd_perf(struct ffps_perf *inf, uint flags);

/** Get the difference between two perf data objects. */
FF_EXTN void ffps_perf_diff(const struct ffps_perf *start, struct ffps_perf *stop);

/** Add up two perf data objects. */
FF_EXTN void ffps_perf_add(struct ffps_perf *dst, const struct ffps_perf *src);

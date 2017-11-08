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

/** Get filename of the current process. */
FF_EXTN const char* ffps_filename(char *name, size_t cap, const char *argv0);

/** Create a copy of the current process in background.
Return child process descriptor (parent);  0 (child);  -1 on error. */
FF_EXTN ffps ffps_createself_bg(const char *arg);

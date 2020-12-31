/** ffos: process
2020, Simon Zolin
*/

/*
Descriptor:
	ffps_exec ffps_exec_info
	ffps_id
	ffps_wait ffps_kill ffps_sig ffps_close
Current:
	ffps_curid
	ffps_curhdl
	ffps_exit
	ffps_filename
*/

#pragma once

#include <FFOS/string.h>

typedef struct ffps_execinfo {
	/** List of process arguments: ["process", "arg1", "arg2", NULL]
	Windows: double-quotes in arguments are not escaped */
	const char **argv;

	/** Set working directory */
	const char *workdir;

	/** List of environment variables
	Windows: not implemented */
	const char **env;

	/** Standard file descriptors */
	fffd in, out, err;
} ffps_execinfo;


#ifdef FF_WIN

#include <ffbase/stringz.h>

typedef HANDLE ffps;
#define FFPS_NULL  INVALID_HANDLE_VALUE

static inline ffuint ffps_id(ffps ps)
{
	return GetProcessId(ps);
}

/** Convert argv[] to command line string
[ "filename", "arg space", "arg", NULL ] -> "filename \"arg space\" arg"
argv:
  Must not contain double quotes
  Strings containing spaces are enclosed in double quotes
Return a newly allocated string */
static wchar_t* _ffargv_to_cmdln(const char **argv)
{
	ffsize cap = 1;
	for (ffsize i = 0;  argv[i] != NULL;  i++) {
		ffssize r;
		if (0 >= (r = ffsz_utow(NULL, 0, argv[i]))) {
			SetLastError(ERROR_INVALID_PARAMETER);
			return NULL;
		}
		cap += r + FFS_LEN("\"\" ");
	}

	wchar_t *args;
	if (NULL == (args = (wchar_t*)ffmem_alloc((cap + 1) * sizeof(wchar_t))))
		return NULL;

	ffsize k = 0;
	for (ffsize i = 0;  argv[i] != NULL;  i++) {

		if (0 <= ffsz_findchar(argv[i], '"')) {
			ffmem_free(args);
			SetLastError(ERROR_INVALID_PARAMETER);
			return NULL;
		}

		ffbool has_space = (0 <= ffsz_findchar(argv[i], ' '));
		if (has_space)
			args[k++] = '"';

		k += ffsz_utow(&args[k], cap - k, argv[i]) - 1;

		if (has_space)
			args[k++] = '"';

		args[k++] = ' ';
	}
	if (k != 0)
		k--; // trim ' '
	args[k++] = '\0';

	return args;
}

/** Start a new process with the specified command line */
static ffps _ffps_exec_cmdln(const wchar_t *filename, wchar_t *cmdln, ffps_execinfo *i)
{
	STARTUPINFOW si = {};
	si.cb = sizeof(STARTUPINFO);

	if (i->in != INVALID_HANDLE_VALUE) {
		si.hStdInput = i->in;
		SetHandleInformation(i->in, HANDLE_FLAG_INHERIT, 1);
	}
	if (i->out != INVALID_HANDLE_VALUE) {
		si.hStdOutput = i->out;
		SetHandleInformation(i->out, HANDLE_FLAG_INHERIT, 1);
	}
	if (i->err != INVALID_HANDLE_VALUE) {
		si.hStdError = i->err;
		SetHandleInformation(i->err, HANDLE_FLAG_INHERIT, 1);
	}
	int inherit_handles = 0;
	if (i->in != INVALID_HANDLE_VALUE || i->out != INVALID_HANDLE_VALUE || i->err != INVALID_HANDLE_VALUE) {
		si.dwFlags |= STARTF_USESTDHANDLES;
		inherit_handles = 1;
	}

	const ffuint f = CREATE_UNICODE_ENVIRONMENT;
	PROCESS_INFORMATION info;
	BOOL b = CreateProcessW(filename, cmdln, NULL, NULL, inherit_handles, f, /*env*/ NULL
		, /*startup dir*/ NULL, &si, &info);

	if (i->in != INVALID_HANDLE_VALUE) {
		SetHandleInformation(i->in, HANDLE_FLAG_INHERIT, 0);
	}
	if (i->out != INVALID_HANDLE_VALUE) {
		SetHandleInformation(i->out, HANDLE_FLAG_INHERIT, 0);
	}
	if (i->err != INVALID_HANDLE_VALUE) {
		SetHandleInformation(i->err, HANDLE_FLAG_INHERIT, 0);
	}

	if (!b)
		return FFPS_NULL;
	CloseHandle(info.hThread);
	return info.hProcess;
}

static inline ffps ffps_exec_info(const char *filename, ffps_execinfo *info)
{
	ffps ps = FFPS_NULL;
	wchar_t wfn_s[256], *wfn, *args;
	if (NULL == (wfn = ffsz_alloc_buf_utow(wfn_s, FF_COUNT(wfn_s), filename)))
		return NULL;

	if (NULL == (args = _ffargv_to_cmdln(info->argv)))
		goto end;

	ps = _ffps_exec_cmdln(wfn, args, info);

end:
	if (wfn != wfn_s)
		ffmem_free(wfn);
	ffmem_free(args);
	return ps;
}

static inline int ffps_wait(ffps ps, ffuint timeout_ms, int *exit_code)
{
	int r = WaitForSingleObject(ps, timeout_ms);
	if (r == WAIT_OBJECT_0) {
		if (exit_code != NULL)
			GetExitCodeProcess(ps, (DWORD*)exit_code);
		CloseHandle(ps);
		r = 0;
	} else if (r == WAIT_TIMEOUT) {
		SetLastError(WSAETIMEDOUT);
	}
	return r;
}

static inline int ffps_kill(ffps ps)
{
	return !TerminateProcess(ps, 9 /*SIGKILL*/);
}

static inline void ffps_close(ffps ps)
{
	CloseHandle(ps);
}

static inline ffps ffps_curhdl()
{
	return GetCurrentProcess();
}

static inline ffuint ffps_curid()
{
	return GetCurrentProcessId();
}

static inline void ffps_exit(int status)
{
	ExitProcess(status);
}

static inline const char* ffps_filename(char *name, ffsize cap, const char *argv0)
{
	(void)argv0;
	const char *r = NULL;
	wchar_t ws[256], *w = ws;
	ffsize n = GetModuleFileNameW(NULL, ws, FF_COUNT(ws));
	if (n == 0)
		return NULL;
	if (n == FF_COUNT(ws)) {
		n = 4096 * 2;
		if (NULL == (w = (wchar_t*)ffmem_alloc(n)))
			return NULL;

		n = GetModuleFileNameW(NULL, w, n);
		if (n == 0)
			goto end;
	}

	n = ffsz_wtou(name, cap, w);
	if ((ffssize)n < 0)
		goto end;

	r = name;

end:
	if (w != ws)
		ffmem_free(w);
	return r;
}

#else // UNIX:

#include <sys/wait.h>
#include <signal.h>
#include <limits.h>
#include <stdlib.h>
#include <errno.h>
#ifdef FF_APPLE
	#include <mach-o/dyld.h>
#endif

typedef int ffps;
#define FFPS_NULL  (-1)

static inline ffps ffps_exec_info(const char *filename, ffps_execinfo *info)
{
	pid_t p = vfork();
	if (p != 0)
		return p;

	struct sigaction sa = {};
	sa.sa_handler = SIG_DFL;
	sigaction(SIGPIPE, &sa, NULL);

	if (info->in != -1)
		dup2(info->in, STDIN_FILENO);
	if (info->out != -1)
		dup2(info->out, STDOUT_FILENO);
	if (info->err != -1)
		dup2(info->err, STDERR_FILENO);

	if (info->workdir != NULL && 0 != chdir(info->workdir))
		goto end;

	execve(filename, (char**)info->argv, (char**)info->env);

end:
	_exit(255);
	return 0;
}

static inline int ffps_wait(ffps ps, ffuint timeout_ms, int *exit_code)
{
	siginfo_t s;
	int r, f = 0;

	if (timeout_ms != (ffuint)-1) {
		f = WNOHANG;
		s.si_pid = 0;
	}

	do {
		r = waitid(P_PID, (id_t)ps, &s, WEXITED | f);
	} while (r != 0 && errno == EINTR);
	if (r != 0)
		return -1;

	if (s.si_pid == 0) {
		errno = ETIMEDOUT;
		return -1;
	}

	if (exit_code != NULL) {
		if (s.si_code == CLD_EXITED)
			*exit_code = s.si_status;
		else
			*exit_code = -s.si_status;
	}
	return 0;
}

/** Process fork
Return
  child PID
  0 in the child process
  -1 on error */
#define ffps_fork  fork

static inline ffuint ffps_id(ffps ps)
{
	return ps;
}

static inline int ffps_kill(ffps ps)
{
	return kill(ps, SIGKILL);
}

/** Send signal to a process */
static inline int ffps_sig(ffps ps, int sig)
{
	return kill(ps, sig);
}

static inline void ffps_close(ffps ps)
{
	(void)ps;
}

static inline ffps ffps_curhdl()
{
	return getpid();
}

static inline ffuint ffps_curid()
{
	return getpid();
}

static inline void ffps_exit(int status)
{
	_exit(status);
}

/* Note: $PATH should be used to find a file in case argv0 is without path, e.g. "binary" */
static inline const char* _ffpath_real(char *name, ffsize cap, const char *argv0)
{
	char real[PATH_MAX];
	if (NULL == realpath(argv0, real))
		return NULL;

	ffuint n = ffsz_len(real) + 1;
	if (n > cap)
		return NULL;

	ffmem_copy(name, real, n);
	return name;
}

static inline const char* ffps_filename(char *name, ffsize cap, const char *argv0)
{
#ifdef FF_LINUX

	int n = readlink("/proc/self/exe", name, cap);
	if (n >= 0) {
		name[n] = '\0';
		return name;
	}

	return _ffpath_real(name, cap, argv0);

#elif defined FF_BSD

#ifdef KERN_PROC_PATHNAME
	static const int sysctl_pathname[] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, /*PID=*/ -1 };
	ffsize namelen = cap;
	if (0 == sysctl(sysctl_pathname, FF_COUNT(sysctl_pathname), name, &namelen, NULL, 0))
		return name;
#endif

	int n = readlink("/proc/curproc/file", name, cap);
	if (n >= 0) {
		name[n] = '\0';
		return name;
	}

	n = readlink("/proc/curproc/exe", name, cap);
	if (n >= 0) {
		name[n] = '\0';
		return name;
	}

	return _ffpath_real(name, cap, argv0);

#elif defined FF_APPLE

	char fn[4096];
	ffuint n = sizeof(fn);
	if (0 == _NSGetExecutablePath(fn, &n))
		argv0 = fn;

	return _ffpath_real(name, cap, argv0);

#endif
}

#endif


/** Create a new process
Return process descriptor (close with ffps_close())
  FFPS_NULL on error */
static ffps ffps_exec_info(const char *filename, ffps_execinfo *info);

static inline ffps ffps_exec(const char *filename, const char **argv, const char **env)
{
	ffps_execinfo info = {};
	info.argv = argv;
	info.env = env;
#ifdef FF_WIN
	info.in = info.out = info.err = INVALID_HANDLE_VALUE;
#else
	info.in = info.out = info.err = -1;
#endif
	return ffps_exec_info(filename, &info);
}

/** Wait for the process to exit
UNIX: EINTR is handled automatically
timeout_ms: Maximum time to wait
  >=0: check status (don't wait)
  -1: wait forever
Return 0 on success.  Process handle is closed, so don't call ffps_close() in this case. */
static int ffps_wait(ffps ps, ffuint timeout_ms, int *exit_code);

/** Kill the process */
static int ffps_kill(ffps ps);

/** Close process handle */
static void ffps_close(ffps ps);

/** Get process ID by process descriptor */
static ffuint ffps_id(ffps ps);


/** Get ID of the current process */
static ffuint ffps_curid();

/** Get the current process handle
Don't call ffps_close() on this handle */
static ffps ffps_curhdl();

/** Exit the current process */
static void ffps_exit(int status);

/** Get filename of the current process
Return NULL on error */
static const char* ffps_filename(char *name, ffsize cap, const char *argv0);


#ifndef FFOS_NO_COMPAT
#include <FFOS/process-compat.h>
#endif

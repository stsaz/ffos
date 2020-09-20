/** ffos: get backtrace info
2020, Simon Zolin
*/

/*
ffthread_backtrace
ffthread_backtrace_frame
ffthread_backtrace_modbase
ffthread_backtrace_modname
ffthread_backtrace_print
*/

#pragma once

#include <FFOS/file.h>
#include <ffbase/string.h> // optional

#ifdef FF_WIN

typedef struct ffthread_bt {
	void *frames[64];
	ffuint n;
	int i_mod;
	void *base;
	wchar_t wname[1024];
} ffthread_bt;

static inline int ffthread_backtrace(ffthread_bt *bt)
{
	ffuint n = FF_COUNT(bt->frames);
#if FF_WIN <= 0x0501
	n = ffmin(62, FF_COUNT(bt->frames));
#endif
	bt->n = CaptureStackBackTrace(0, n, bt->frames, NULL);
	bt->i_mod = -1;
	return bt->n;
}

static inline void* ffthread_backtrace_modbase(ffthread_bt *bt, ffuint i)
{
	if (i >= bt->n)
		return NULL;

	if (i != (ffuint)bt->i_mod) {
		HMODULE mod;
		ffuint f = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
			| GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
		if (!GetModuleHandleEx(f, (wchar_t*)bt->frames[i], &mod))
			return NULL;
		bt->base = mod;
		bt->i_mod = i;
	}

	return bt->base;
}

static inline const wchar_t* ffthread_backtrace_modname(ffthread_bt *bt, ffuint i)
{
	if (i >= bt->n || bt->frames[i] == NULL)
		return NULL;

	if (i != (ffuint)bt->i_mod) {
		HMODULE mod;
		ffuint f = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
			| GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
		if (!GetModuleHandleEx(f, (wchar_t*)bt->frames[i], &mod))
			return NULL;
		bt->base = mod;
		bt->i_mod = i;
	}

	bt->wname[0] = '\0';
	(void) GetModuleFileNameW((HMODULE)bt->base, bt->wname, FF_COUNT(bt->wname));
	return bt->wname;
}

#else // unix:

#include <execinfo.h>
#include <dlfcn.h>

typedef struct ffthread_bt {
	void *frames[64];
	ffuint n;
	Dl_info info;
	int i_info;
} ffthread_bt;

static inline int ffthread_backtrace(ffthread_bt *bt)
{
	bt->n = backtrace(bt->frames, FF_COUNT(bt->frames));
	bt->i_info = -1;
	return bt->n;
}

static inline void* ffthread_backtrace_modbase(ffthread_bt *bt, ffuint i)
{
	if (i >= bt->n)
		return NULL;

	if (i != (ffuint)bt->i_info) {
		if (0 == dladdr(bt->frames[i], &bt->info))
			return NULL;
		bt->i_info = i;
	}

	return bt->info.dli_fbase;
}

static inline const char* ffthread_backtrace_modname(ffthread_bt *bt, ffuint i)
{
	if (i >= bt->n || bt->frames[i] == NULL)
		return NULL;

	if (i != (ffuint)bt->i_info) {
		if (0 == dladdr(bt->frames[i], &bt->info))
			return NULL;
		bt->i_info = i;
	}

	return bt->info.dli_fname;
}

#endif

static inline void* ffthread_backtrace_frame(ffthread_bt *bt, ffuint i)
{
	return bt->frames[i];
}

#ifdef _FFBASE_STRING_H
static inline void ffthread_backtrace_print(fffd fd, ffuint limit)
{
	ffthread_bt bt = {};
	ffuint n = ffthread_backtrace(&bt);
	if (limit != 0 && limit < n)
		n = limit;

	ffsize cap = n * 80;
	ffstr s = {};
	ffstr_alloc(&s, cap);

	for (ffuint i = 0;  i != n;  i++) {
		ffsize offset = (char*)ffthread_backtrace_frame(&bt, i) - (char*)ffthread_backtrace_modbase(&bt, i);
		ffstr_growfmt(&s, &cap, " #%u 0x%p +%xL %q [0x%p]\n"
			, i
			, ffthread_backtrace_frame(&bt, i)
			, offset
			, ffthread_backtrace_modname(&bt, i)
			, ffthread_backtrace_modbase(&bt, i)
			);
	}

	fffile_write(fd, s.ptr, s.len);
	ffstr_free(&s);
}
#endif


/** Get backtrace info */
static int ffthread_backtrace(ffthread_bt *bt);

/** Get frame pointer */
static void* ffthread_backtrace_frame(ffthread_bt *bt, ffuint i);

/** Get module base address
Return NULL on error */
static void* ffthread_backtrace_modbase(ffthread_bt *bt, ffuint i);

/** Get module path
Return NULL on error */
static const ffsyschar* ffthread_backtrace_modname(ffthread_bt *bt, ffuint i);

/** Print backtrace to fd (e.g. ffstdout) */
static void ffthread_backtrace_print(fffd fd, ffuint limit);


#define ffthd_bt  ffthread_bt
#define ffthd_backtrace  ffthread_backtrace
#define ffthd_backtrace_frame  ffthread_backtrace_frame
#define ffthd_backtrace_modbase  ffthread_backtrace_modbase
#define ffthd_backtrace_modname  ffthread_backtrace_modname
#define ffthd_backtrace_print  ffthread_backtrace_print

/**
Copyright (c) 2020 Simon Zolin
*/

#include <execinfo.h>
#include <dlfcn.h>


typedef struct ffthd_bt {
	void *frames[64];
	uint n;
	Dl_info info;
	int i_info;
} ffthd_bt;

static inline int ffthd_backtrace(ffthd_bt *bt)
{
	bt->n = backtrace(bt->frames, FFCNT(bt->frames));
	bt->i_info = -1;
	return bt->n;
}

static inline void* ffthd_backtrace_modbase(ffthd_bt *bt, uint i)
{
	if (i >= bt->n)
		return NULL;

	if (i != (uint)bt->i_info) {
		if (0 == dladdr(bt->frames[i], &bt->info))
			return NULL;
		bt->i_info = i;
	}

	return bt->info.dli_fbase;
}

static inline const char* ffthd_backtrace_modname(ffthd_bt *bt, uint i)
{
	if (i >= bt->n || bt->frames[i] == NULL)
		return NULL;

	if (i != (uint)bt->i_info) {
		if (0 == dladdr(bt->frames[i], &bt->info))
			return NULL;
		bt->i_info = i;
	}

	return bt->info.dli_fname;
}

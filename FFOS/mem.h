/** Memory functions.
Copyright (c) 2013 Simon Zolin
*/

#pragma once

#include <FFOS/types.h>

#ifdef FF_WIN
#include <FFOS/win/mem.h>
#else
#include <FFOS/unix/mem.h>
#endif

#include <string.h>

/** Allocate N objects of type T. */
#define ffmem_talloc(T, N)  ((T*)ffmem_alloc((N) * sizeof(T)))

/** Allocate N objects of type T.  Zero the buffer. */
#define ffmem_tcalloc(T, N)  ((T*)ffmem_calloc(N, sizeof(T)))

/** Allocate an object of type T. */
#define ffmem_tcalloc1(T)  ((T*)ffmem_calloc(1, sizeof(T)))

/** Zero the buffer. */
#define ffmem_zero(p, len)  memset(p, 0, len)

/** Zero the object. */
#define ffmem_tzero(p)  memset(p, 0, sizeof(*(p)))


#ifdef FFDBG_MEM
static FFINL void * ffmem_alloc(size_t size) {
	void *p = _ffmem_alloc(size);
	ffdbg_print(0, "%s(): p:%p, size:%L\n"
		, FF_FUNC, p, size);
	return p;
}

static FFINL void * ffmem_calloc(size_t n, size_t sz) {
	void *p = _ffmem_calloc(n, sz);
	ffdbg_print(0, "%s(): p:%p, size:%L*%L\n"
		, FF_FUNC, p, n, sz);
	return p;
}

static FFINL void * ffmem_realloc(void *ptr, size_t newSize) {
	void *p;
	p = _ffmem_realloc(ptr, newSize);
	if (p == ptr)
		ffdbg_print(0, "%s(): p:%p, size:%L\n"
			, FF_FUNC, p, newSize, ptr);
	else
		ffdbg_print(0, "%s(): p:%p, size:%L, oldp:%p\n"
			, FF_FUNC, p, newSize, ptr);
	return p;
}

static FFINL void ffmem_free(void *ptr) {
	ffdbg_print(0, "%s(): p:%p\n"
		, FF_FUNC, ptr);
	_ffmem_free(ptr);
}

#else
#define ffmem_alloc _ffmem_alloc
#define ffmem_calloc _ffmem_calloc
#define ffmem_realloc _ffmem_realloc
#define ffmem_free _ffmem_free

#endif

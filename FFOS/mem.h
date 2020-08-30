/** Memory functions.
Copyright (c) 2013 Simon Zolin
*/

/*
ffmem_alloc ffmem_calloc ffmem_realloc ffmem_free
ffmem_align ffmem_alignfree
*/

#pragma once

#include <FFOS/types.h>
#include <string.h>

#ifdef FF_WIN

static inline void* ffmem_alloc(ffsize size)
{
	return HeapAlloc(GetProcessHeap(), 0, size);
}

static inline void* ffmem_calloc(ffsize n, ffsize elsize)
{
	return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, n * elsize);
}

static inline void* ffmem_realloc(void *ptr, ffsize new_size)
{
	if (ptr == NULL)
		return HeapAlloc(GetProcessHeap(), 0, new_size);
	return HeapReAlloc(GetProcessHeap(), 0, ptr, new_size);
}

static inline void ffmem_free(void *ptr)
{
	HeapFree(GetProcessHeap(), 0, ptr);
}

/* (allocated-start) (free space) (pointer to allocated-start) (aligned) ... (allocated-end) */
static inline void* ffmem_align(ffsize size, ffsize align)
{
	if ((align % sizeof(void*)) != 0) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return NULL;
	}

	void *buf;
	if (NULL == (buf = ffmem_alloc(size + align + sizeof(void*))))
		return NULL;

	void *al = (void*)(ffsize)ff_align_ceil2((ffsize)buf + sizeof(void*), align);
	*((void**)al - 1) = buf; // remember the original pointer
	return al;
}

static inline void ffmem_alignfree(void *ptr)
{
	if (ptr == NULL)
		return;

	void *buf = *((void**)ptr - 1);
	ffmem_free(buf);
}

#else // UNIX:

#include <stdlib.h>
#include <errno.h>

static inline void* ffmem_alloc(ffsize size)
{
	return malloc(size);
}

static inline void* ffmem_calloc(ffsize n, ffsize elsize)
{
	return calloc(n, elsize);
}

static inline void* ffmem_realloc(void *ptr, ffsize new_size)
{
	return realloc(ptr, new_size);
}

static inline void ffmem_free(void *ptr)
{
	free(ptr);
}

static inline void* ffmem_align(ffsize size, ffsize align)
{
#ifdef FF_ANDROID
	return NULL;

#else
	void *buf;
	int e = posix_memalign(&buf, align, size);
	if (e != 0) {
		errno = e;
		return NULL;
	}
	return buf;
#endif
}

static inline void ffmem_alignfree(void *ptr)
{
	free(ptr);
}

#endif


/** Allocate heap memory region
Return NULL on error */
static void* ffmem_alloc(ffsize size);

/** Allocate heap memory zero-filled region
Return NULL on error */
static void* ffmem_calloc(ffsize n, ffsize elsize);

/** Reallocate heap memory region
Return NULL on error */
static void* ffmem_realloc(void *ptr, ffsize new_size);

/** Allocate an object of type T */
#define ffmem_new(T)  ((T*)ffmem_calloc(1, sizeof(T)))

/** Deallocate heap memory region */
static void ffmem_free(void *ptr);


/** Allocate aligned memory
Android: not implemented
align:
  Windows: must be a multiple of sizeof(void*)
Return NULL on error */
static void* ffmem_align(ffsize size, ffsize align);

/** Deallocate aligned memory */
static void ffmem_alignfree(void *ptr);


#include <FFOS/mem-compat.h>

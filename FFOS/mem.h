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
#define ffmem_allocT(N, T)  ((T*)ffmem_alloc((N) * sizeof(T)))

/** Allocate N objects of type T.  Zero the buffer. */
#define ffmem_tcalloc(T, N)  ((T*)ffmem_calloc(N, sizeof(T)))
#define ffmem_callocT(N, T)  ((T*)ffmem_calloc(N, sizeof(T)))

/** Allocate an object of type T. */
#define ffmem_tcalloc1(T)  ((T*)ffmem_calloc(1, sizeof(T)))
#define ffmem_new(T)  ((T*)ffmem_calloc(1, sizeof(T)))

/** Zero the buffer. */
#define ffmem_zero(p, len)  memset(p, 0, len)

/** Zero the object. */
#define ffmem_tzero(p)  memset(p, 0, sizeof(*(p)))


#ifdef FFMEM_DBG
FF_EXTN void* ffmem_alloc(size_t size);
FF_EXTN void* ffmem_calloc(size_t n, size_t size);
FF_EXTN void* ffmem_realloc(void *ptr, size_t newsize);
FF_EXTN void ffmem_free(void *ptr);

FF_EXTN void* ffmem_align(size_t size, size_t align);
FF_EXTN void ffmem_alignfree(void *ptr);

#else
#define ffmem_alloc _ffmem_alloc
#define ffmem_calloc _ffmem_calloc
#define ffmem_realloc _ffmem_realloc
#define ffmem_free _ffmem_free

#define ffmem_align _ffmem_align
#define ffmem_alignfree _ffmem_alignfree
#endif

/** Safely reallocate memory buffer.
Return NULL on error and free the original buffer. */
static FFINL void* ffmem_saferealloc(void *ptr, size_t newsize)
{
	void *p = ffmem_realloc(ptr, newsize);
	if (p == NULL) {
		ffmem_free(ptr);
		return NULL;
	}
	return p;
}

#define ffmem_safefree(p) \
do { \
	if (p != NULL) \
		ffmem_free(p); \
} while (0)

#define ffmem_safefree0(p)  FF_SAFECLOSE(p, NULL, ffmem_free)

#define ffmem_free0(p) \
do { \
	ffmem_free(p); \
	p = NULL; \
} while (0)

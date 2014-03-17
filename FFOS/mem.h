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

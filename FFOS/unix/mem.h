/**
Copyright (c) 2013 Simon Zolin
*/

#include <stdlib.h>

/** Initialize FFOS. */
#define ffos_init()

/** Allocate heap memory region. */
#define ffmem_alloc  malloc

#define ffmem_calloc  calloc

/** Reallocate heap memory region. */
#define ffmem_realloc  realloc

/** Deallocate heap memory region. */
#define ffmem_free  free

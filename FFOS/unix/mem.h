/**
Copyright (c) 2013 Simon Zolin
*/

#include <stdlib.h>
#include <locale.h>

/** Initialize FFOS. */
static FFINL void ffos_init() {
	setlocale(LC_CTYPE, "en_US.UTF-8"); //for ff_wtou()
}

/** Allocate heap memory region. */
#define ffmem_alloc  malloc

#define ffmem_calloc  calloc

/** Reallocate heap memory region. */
#define ffmem_realloc  realloc

/** Deallocate heap memory region. */
#define ffmem_free  free

/**
Copyright (c) 2013 Simon Zolin
*/

#pragma once

#include <FFOS/types.h>

#ifdef FF_WIN
#include <FFOS/win/mem.h>
#else
#include <FFOS/unix/mem.h>
#endif

#define ffmem_talloc(T, N)  (T*)ffmem_alloc(N * sizeof(T))

#define ffmem_tcalloc(T, N)  (T*)ffmem_calloc(N, sizeof(T))

#define ffmem_tcalloc1(T)  (T*)ffmem_calloc(1, sizeof(T))

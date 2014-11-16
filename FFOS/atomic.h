/** Atomic operations.
Copyright (c) 2013 Simon Zolin
*/

#pragma once

#include <FFOS/types.h>


/** Set new value. */
#define ffatom_set(a, val)  *(a) = (val)

/** Get value. */
#define ffatom_get(a)  (*(a))


#ifdef FF_UNIX
#include <FFOS/unix/atomic.h>
#else
#include <FFOS/win/atomic.h>
#endif


typedef struct fflock {
	ffatomic lock;
} fflock;

FF_EXTN uint _ffsc_ncpu;

/** Initialize fflock object. */
#define fflk_init(lk) \
	(lk)->lock = 0

/** Acquire spinlock. */
FF_EXTN void fflk_lock(fflock *lk);

/** Return FALSE if lock couldn't be acquired. */
#define fflk_trylock(lk) \
	(0 == ffatom_get(&(lk)->lock) && ffatom_cmpxchg(&(lk)->lock, 0, 1))

/** Release lock. */
#define fflk_unlock(lk) \
	ffatom_set(&(lk)->lock, 0)

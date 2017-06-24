/** Atomic operations.
Copyright (c) 2013 Simon Zolin
*/

#pragma once

#include <FFOS/types.h>


#define ffatom_getT(a, T)  (*(volatile T*)(a))
#define ffatom_setT(a, set, T)  (*(volatile T*)(a) = (set))


#ifdef FF_UNIX
#include <FFOS/unix/atomic.h>
#else
#include <FFOS/win/atomic.h>
#endif


typedef struct fflock {
	ffatomic lock;
} fflock;

FF_EXTN uint _ffsc_ncpu;

/** Store the number of active CPUs for spinlock. */
FF_EXTN void fflk_setup(void);

/** Initialize fflock object. */
#define fflk_init(lk) \
	ffatom_set(&(lk)->lock, 0)

/** Acquire spinlock. */
FF_EXTN void fflk_lock(fflock *lk);

/** Return FALSE if lock couldn't be acquired. */
#define fflk_trylock(lk) \
	(0 == ffatom_get(&(lk)->lock) && ffatom_cmpxchg(&(lk)->lock, 0, 1))

/** Release lock. */
#define fflk_unlock(lk) \
	ffatom_set(&(lk)->lock, 0)

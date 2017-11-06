/** Atomic operations.
Copyright (c) 2013 Simon Zolin
*/

#pragma once

#include <FFOS/cpu.h>


/** Set new value. */
#define ffatom_set(a, set)  FF_WRITEONCE(&(a)->val, set)

/** Get value. */
#define ffatom_get(a)  FF_READONCE(&(a)->val)

/** Add integer and return new value. */
static FFINL size_t ffatom_addret(ffatomic *a, size_t add)
{
	return ffatom_fetchadd(a, add) + add;
}

/** Increment and return new value. */
#define ffatom_incret(a)  ffatom_addret(a, 1)

/** Decrement and return new value. */
#define ffatom_decret(a)  ffatom_addret(a, -1)

/** Switch CPU to another task. */
#ifdef FF_WIN
#define ffcpu_yield  SwitchToThread
#else
#include <sched.h>
#define ffcpu_yield  sched_yield
#endif

/** Block execution until the variable is modified by another thread. */
FF_EXTN uint ffatom_waitchange(uint *p, uint curval);


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
static FFINL int fflk_trylock(fflock *lk)
{
	return ffatom_cmpset(&lk->lock, 0, 1);
}

/** Release lock. */
static FFINL void fflk_unlock(fflock *lk)
{
	ffatom_fence_rel();
	ffatom_set(&lk->lock, 0);
}

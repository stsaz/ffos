/** Atomic operations.
Copyright (c) 2013 Simon Zolin
*/

#pragma once

#include <FFOS/cpu.h>
#include <ffbase/lock.h>


/** Set new value. */
#define ffatom_set(a, set)  FF_WRITEONCE((a)->val, set)

/** Get value. */
#define ffatom_get(a)  FF_READONCE((a)->val)

/** Add integer and return new value. */
static FFINL size_t ffatom_addret(ffatomic *a, size_t add)
{
	return ffatom_fetchadd(a, add) + add;
}

static FFINL uint ffatom32_addret(ffatomic32 *a, uint add)
{
	return ffatom32_fetchadd(a, add) + add;
}

/** Increment and return new value. */
#define ffatom_incret(a)  ffatom_addret(a, 1)

/** Decrement and return new value. */
#define ffatom_decret(a)  ffatom_addret(a, -1)
#define ffatom32_decret(a)  ffatom32_addret(a, -1)

/** Block execution until the variable is modified by another thread. */
static inline ffuint ffatom_waitchange(ffuint *p, ffuint curval)
{
	ffuint r;

	for (;;) {
		if (curval != (r = FFINT_READONCE(*p)))
			return r;

		for (ffuint n = 0;  n != 2048;  n++) {
			ffcpu_pause();
			if (curval != (r = FFINT_READONCE(*p)))
				return r;
		}

		ffthread_yield();
	}
}


#define ffcpu_yield  ffthread_yield
#define fflk_setup()
#define fflk_init  fflock_init
#define fflk_lock  fflock_lock
#define fflk_trylock  fflock_trylock
#define fflk_unlock  fflock_unlock

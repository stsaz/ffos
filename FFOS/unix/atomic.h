/**
Copyright (c) 2013 Simon Zolin
*/

#include <sched.h>


typedef ssize_t ffatomic;

#define LOCK  "lock; "

/** Set new value and return old value. */
static FFINL ssize_t ffatom_xchg(ffatomic *a, ssize_t val) {
	asm volatile(
		// no lock
		"xchg %0, %1\n"
		: "+r" (val), "+m" (*a)
		: : "memory", "cc");
	return val;
}

/** Compare and, if equal, set new value.
Return 1 if equal. */
static FFINL int ffatom_cmpxchg(ffatomic *a, ssize_t old, ssize_t newval) {
	ssize_t ret;
	asm volatile(
		LOCK "cmpxchg %2, %1\n"
		: "=a" (ret), "+m" (*a)
		: "r" (newval), "0" (old)
		: "memory", "cc");
	return ret == old;
}


/** Add integer. */
static FFINL void ffatom_add(ffatomic *a, ssize_t add) {
	asm volatile(
		LOCK "add %1, %0;"
		: "+m" (*a)
		: "ir" (add));
}

/** Increment. */
static FFINL void ffatom_inc(ffatomic *a) {
#ifdef FF_64
	asm volatile(
		LOCK "incq %0"
		: "+m" (*a));

#else
	asm volatile(
		LOCK "incl %0"
		: "+m" (*a));
#endif
}

/** Decrement. */
static FFINL void ffatom_dec(ffatomic *a) {
#ifdef FF_64
	asm volatile(
		LOCK "decq %0"
		: "+m" (*a));

#else
	asm volatile(
		LOCK "decl %0"
		: "+m" (*a));
#endif
}

/** Add integer and return new value. */
static FFINL ssize_t ffatom_addret(ffatomic *a, ssize_t add) {
	ssize_t r = add;
	asm volatile(
		LOCK "xadd %0, %1;"
		: "+r" (r), "+m" (*a)
		: : "memory", "cc");
	return r + add;
}

/** Increment and return new value. */
#define ffatom_incret(a)  ffatom_addret(a, 1)

/** Decrement and return new value. */
#define ffatom_decret(a)  ffatom_addret(a, -1)

#undef LOCK


/** Optimization barrier. */
#define ffmem_barrier()  asm volatile ("" : : : "memory")

#define ffcpu_pause()  asm ("pause")

/** Switch CPU to another task. */
#define ffcpu_yield  sched_yield

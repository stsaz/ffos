/**
Copyright (c) 2013 Simon Zolin
*/

#include <sched.h>


typedef struct { ssize_t val; } ffatomic;

/** Set new value. */
#define ffatom_set(a, set)  ffatom_setT(&(a)->val, set, __typeof__((a)->val))

/** Get value. */
#define ffatom_get(a)  ffatom_getT(&(a)->val, __typeof__((a)->val))

#define LOCK  "lock; "

/** Set new value and return old value. */
static FFINL ssize_t ffatom_xchg(ffatomic *a, ssize_t val) {
	__asm__ volatile(
		// no lock
		"xchg %0, %1\n"
		: "+r" (val), "+m" (a->val)
		: : "memory", "cc");
	return val;
}

/** Compare and, if equal, set new value.
Return 1 if equal. */
static FFINL int ffatom_cmpxchg(ffatomic *a, ssize_t old, ssize_t newval) {
	ssize_t ret;
	__asm__ volatile(
		LOCK "cmpxchg %2, %1\n"
		: "=a" (ret), "+m" (a->val)
		: "r" (newval), "0" (old)
		: "memory", "cc");
	return ret == old;
}


/** Add integer. */
static FFINL void ffatom_add(ffatomic *a, ssize_t add) {
	__asm__ volatile(
		LOCK "add %1, %0;"
		: "+m" (a->val)
		: "ir" (add));
}

/** Increment. */
static FFINL void ffatom_inc(ffatomic *a) {
#ifdef FF_64
	__asm__ volatile(
		LOCK "incq %0"
		: "+m" (a->val));

#else
	__asm__ volatile(
		LOCK "incl %0"
		: "+m" (a->val));
#endif
}

/** Decrement. */
static FFINL void ffatom_dec(ffatomic *a) {
#ifdef FF_64
	__asm__ volatile(
		LOCK "decq %0"
		: "+m" (a->val));

#else
	__asm__ volatile(
		LOCK "decl %0"
		: "+m" (a->val));
#endif
}

/** Add integer and return new value. */
static FFINL ssize_t ffatom_addret(ffatomic *a, ssize_t add) {
	ssize_t r = add;
	__asm__ volatile(
		LOCK "xadd %0, %1;"
		: "+r" (r), "+m" (a->val)
		: : "memory", "cc");
	return r + add;
}

/** Increment and return new value. */
#define ffatom_incret(a)  ffatom_addret(a, 1)

/** Decrement and return new value. */
#define ffatom_decret(a)  ffatom_addret(a, -1)

static FFINL void ffatom_or(ffatomic *a, ssize_t v)
{
	__asm__ volatile(
		LOCK "or %1, %0;"
		: "+m" (a->val)
		: "ir" (v)
		: "memory");
}

static FFINL void ffatom_and(ffatomic *a, ssize_t v)
{
	__asm__ volatile(
		LOCK "and %1, %0;"
		: "+m" (a->val)
		: "ir" (v)
		: "memory");
}

#undef LOCK


/** Optimization barrier. */
#define ffmem_barrier()  __asm__ volatile ("" : : : "memory")

#define ffcpu_pause()  __asm__ ("pause")

/** Switch CPU to another task. */
#define ffcpu_yield  sched_yield

/** Functions for AMD64.
Copyright (c) 2017 Simon Zolin
*/


/** Set new value and return old value. */
static FFINL size_t ffatom_swap(ffatomic *a, size_t val)
{
	__asm volatile(
		"xchgq %0, %1;"
		: "+r" (val), "+m" (a->val)
		: : "memory", "cc");
	return val;
}

/** Compare and, if equal, set new value.
Return 1 if equal. */
static FFINL int ffatom_cmpset(ffatomic *a, size_t old, size_t newval)
{
	size_t ret;
	__asm volatile(
		LOCK "cmpxchgq %2, %1;"
		: "=a" (ret), "+m" (a->val)
		: "r" (newval), "0" (old)
		: "memory", "cc");
	return ret == old;
}


/** Add integer and return old value. */
static FFINL size_t ffatom_fetchadd(ffatomic *a, size_t add)
{
	__asm volatile(
		LOCK "xaddq %0, %1;"
		: "+r" (add), "+m" (a->val)
		: : "cc");
	return add;
}

/** Add integer. */
static FFINL void ffatom_add(ffatomic *a, size_t add)
{
	__asm volatile(
		LOCK "addq %1, %0;"
		: "+m" (a->val)
		: "er" (add));
}

/** Increment. */
static FFINL void ffatom_inc(ffatomic *a)
{
	__asm volatile(
		LOCK "incq %0;"
		: "+m" (a->val));
}

/** Decrement. */
static FFINL void ffatom_dec(ffatomic *a)
{
	__asm volatile(
		LOCK "decq %0;"
		: "+m" (a->val));
}


static FFINL void ffatom_or(ffatomic *a, size_t v)
{
	__asm volatile(
		LOCK "orq %1, %0;"
		: "+m" (a->val)
		: "er" (v)
		: "memory");
}

static FFINL void ffatom_xor(ffatomic *a, size_t v)
{
	__asm volatile(
		LOCK "xorq %1, %0;"
		: "+m" (a->val)
		: "er" (v)
		: "memory");
}

static FFINL void ffatom_and(ffatomic *a, size_t v)
{
	__asm volatile(
		LOCK "andq %1, %0;"
		: "+m" (a->val)
		: "er" (v)
		: "memory");
}


/** Ensure no "store-load" reorder by CPU (sequential consistency). */
static FFINL void ffatom_fence_seq_cst(void)
{
	__asm volatile(
		LOCK "addl $0,-8(%%rsp);"
		: : : "memory", "cc");
}


static FFINL uint64 ffcpu_rdtsc(void)
{
	uint lo, hi;
	__asm volatile("rdtsc" : "=a" (lo), "=d" (hi));
	return ((uint64)hi << 32) | lo;
}

#undef LOCK

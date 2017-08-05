/** Functions for IA-32.
Copyright (c) 2017 Simon Zolin
*/

#define LOCK  "lock; "

static FFINL size_t ffatom_swap(ffatomic *a, size_t val)
{
	__asm volatile(
		"xchgl %0, %1;"
		: "+r" (val), "+m" (a->val)
		: : "memory", "cc");
	return val;
}

static FFINL int ffatom_cmpset(ffatomic *a, size_t old, size_t newval)
{
	size_t ret;
	__asm volatile(
		LOCK "cmpxchgl %2, %1;"
		: "=a" (ret), "+m" (a->val)
		: "r" (newval), "0" (old)
		: "memory", "cc");
	return ret == old;
}


static FFINL size_t ffatom_fetchadd(ffatomic *a, size_t add)
{
	__asm volatile(
		LOCK "xaddl %0, %1;"
		: "+r" (add), "+m" (a->val)
		: : "cc");
	return add;
}

static FFINL void ffatom_add(ffatomic *a, size_t add)
{
	__asm volatile(
		LOCK "addl %1, %0;"
		: "+m" (a->val)
		: "ir" (add));
}

static FFINL void ffatom_inc(ffatomic *a)
{
	__asm volatile(
		LOCK "incl %0;"
		: "+m" (a->val));
}

static FFINL void ffatom_dec(ffatomic *a)
{
	__asm volatile(
		LOCK "decl %0;"
		: "+m" (a->val));
}

static FFINL void ffatom_or(ffatomic *a, size_t v)
{
	__asm volatile(
		LOCK "orl %1, %0;"
		: "+m" (a->val)
		: "ir" (v)
		: "memory");
}

static FFINL void ffatom_xor(ffatomic *a, size_t v)
{
	__asm volatile(
		LOCK "xorl %1, %0;"
		: "+m" (a->val)
		: "ir" (v)
		: "memory");
}

static FFINL void ffatom_and(ffatomic *a, size_t v)
{
	__asm volatile(
		LOCK "andl %1, %0;"
		: "+m" (a->val)
		: "ir" (v)
		: "memory");
}


#define ffatom_fence_acq()  ff_compiler_fence()

#define ffatom_fence_rel()  ff_compiler_fence()

#define ffatom_fence_acq_rel()  ff_compiler_fence()

static FFINL void ffatom_fence_seq_cst(void)
{
	__asm volatile(
		LOCK "addl $0,(%%esp);"
		: : : "memory", "cc");
}


static FFINL uint64 ffcpu_rdtsc(void)
{
	uint64 r;
	__asm volatile("rdtsc" : "=A" (r));
	return r;
}

#define ffcpu_pause()  __asm volatile("pause")

#undef LOCK

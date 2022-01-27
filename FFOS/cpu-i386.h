/** Functions for IA-32 and AMD64.
Copyright (c) 2017 Simon Zolin
*/


/** Add two numbers with Carry flag: (a + b) + CF. */
static FFINL uint ffint_addcarry32(uint a, uint b)
{
	__asm volatile(
		"addl %2,%0;\n\t"
		"adcl $0,%0;"
		: "=r" (a)
		: "0" (a), "rm" (b));
	return a;
}


#define LOCK  "lock; "

static FFINL size_t ffatom32_swap(ffatomic32 *a, uint val)
{
	__asm volatile(
		"xchgl %0, %1;"
		: "+r" (val), "+m" (a->val)
		: : "memory", "cc");
	return val;
}

static FFINL int ffatom32_cmpset(ffatomic32 *a, uint old, uint newval)
{
	size_t ret;
	__asm volatile(
		LOCK "cmpxchgl %2, %1;"
		: "=a" (ret), "+m" (a->val)
		: "r" (newval), "0" (old)
		: "memory", "cc");
	return ret == old;
}


static FFINL size_t ffatom32_fetchadd(ffatomic32 *a, uint add)
{
	__asm volatile(
		LOCK "xaddl %0, %1;"
		: "+r" (add), "+m" (a->val)
		: : "cc");
	return add;
}

static FFINL void ffatom32_add(ffatomic32 *a, uint add)
{
	__asm volatile(
		LOCK "addl %1, %0;"
		: "+m" (a->val)
		: "ir" (add));
}

static FFINL void ffatom32_inc(ffatomic32 *a)
{
	__asm volatile(
		LOCK "incl %0;"
		: "+m" (a->val));
}

static FFINL void ffatom32_dec(ffatomic32 *a)
{
	__asm volatile(
		LOCK "decl %0;"
		: "+m" (a->val));
}

static FFINL void ffatom32_or(ffatomic32 *a, uint v)
{
	__asm volatile(
		LOCK "orl %1, %0;"
		: "+m" (a->val)
		: "ir" (v)
		: "memory");
}

static FFINL void ffatom32_xor(ffatomic32 *a, uint v)
{
	__asm volatile(
		LOCK "xorl %1, %0;"
		: "+m" (a->val)
		: "ir" (v)
		: "memory");
}

static FFINL void ffatom32_and(ffatomic32 *a, uint v)
{
	__asm volatile(
		LOCK "andl %1, %0;"
		: "+m" (a->val)
		: "ir" (v)
		: "memory");
}


/** Ensure no "load-load" and "load-store" reorder by CPU. */
#define ffatom_fence_acq()  ff_compiler_fence()

/** Ensure no "store-store" and "load-store" reorder by CPU. */
#define ffatom_fence_rel()  ff_compiler_fence()

/** Ensure no "load-load", "load-store" and "store-store" reorder by CPU. */
#define ffatom_fence_acq_rel()  ff_compiler_fence()


#ifndef FF_64

#define ffatom_swap(a, val)  ffatom32_swap((ffatomic32*)a, val)
#define ffatom_cmpset(a, old, newval)  ffatom32_cmpset((ffatomic32*)a, old, newval)
#define ffatom_fetchadd(a, add)  ffatom32_fetchadd((ffatomic32*)a, add)
#define ffatom_add(a, add)  ffatom32_add((ffatomic32*)a, add)
#define ffatom_inc(a)  ffatom32_inc((ffatomic32*)a)
#define ffatom_dec(a)  ffatom32_dec((ffatomic32*)a)
#define ffatom_or(a, v)  ffatom32_or((ffatomic32*)a, v)
#define ffatom_xor(a, v)  ffatom32_xor((ffatomic32*)a, v)
#define ffatom_and(a, v)  ffatom32_and((ffatomic32*)a, v)

static FFINL void ffatom_fence_seq_cst(void)
{
	__asm volatile(
		LOCK "addl $0,(%%esp);"
		: : : "memory", "cc");
}

#undef LOCK

#endif //FF_64

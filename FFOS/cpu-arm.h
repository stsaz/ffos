/** Functions for ARM.
Copyright (c) 2018 Simon Zolin
*/

// to be implemented:

static FFINL uint ffint_addcarry32(uint a, uint b)
{
	FF_ASSERT(0);
	return 0;
}

static FFINL size_t ffatom_fetchadd(ffatomic *a, size_t add)
{
	return __sync_fetch_and_add((volatile size_t*)&a->val, add);
}

static FFINL int ffatom_cmpset(ffatomic *a, size_t old, size_t newval)
{
	return __sync_bool_compare_and_swap((volatile size_t*)&a->val, old, newval);
}

#define ffatom_fence_rel()  __sync_synchronize()
#define ffcpu_pause()

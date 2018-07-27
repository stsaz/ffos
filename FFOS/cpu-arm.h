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
	FF_ASSERT(0);
	return 0;
}

static FFINL int ffatom_cmpset(ffatomic *a, size_t old, size_t newval)
{
	FF_ASSERT(0);
	return 0;
}

#define ffatom_fence_rel()  FF_ASSERT(0)
#define ffcpu_pause()  FF_ASSERT(0)

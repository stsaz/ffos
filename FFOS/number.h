/** Operations with numbers and pointers.
Copyright (c) 2016 Simon Zolin
*/

#pragma once


/** Get offset of a member in structure. */
#define FFOFF(struct_T, member) \
	(((size_t)&((struct_T *)0)->member))

#define FF_SIZEOF(struct_name, member)  sizeof(((struct_name*)0)->member)

#define FF_PTR(p, off)  ((void*)((char*)(p) + (off)))

/** Get structure object by its member. */
#define FF_GETPTR(struct_T, member_name, member_ptr) \
	((struct_T*)((char*)member_ptr - FFOFF(struct_T, member_name)))

/** Set new value and return old value. */
#define FF_SWAP(obj, newval) \
({ \
	__typeof__(*(obj)) _old = *(obj); \
	*(obj) = newval; \
	_old; \
})

#define FF_CMPSET(obj, old, newval) \
	((*(obj) == (old)) ? *(obj) = (newval), 1 : 0)

#define FF_WRITEONCE(a, val)  (*(volatile __typeof__(a))(a) = (val))

#define FF_READONCE(a)  (*(volatile __typeof__(a))(a))

/** Get the number of elements in array. */
#define FFCNT(ar)  (sizeof(ar) / sizeof(*(ar)))


#define FF_BIT32(bit)  (1U << (bit))
#define FF_BIT64(bit)  (1ULL << (bit))

#define FF_LO32(i64)  ((uint)((i64) & 0xffffffff))
#define FF_HI32(i64)  ((uint)(((i64) >> 32) & 0xffffffff))


#define ffabs(n) \
({ \
	__typeof__(n) _n = (n); \
	(_n >= 0) ? _n : -_n; \
})

static FFINL uint64 ffmin64(uint64 a, uint64 b)
{
	return (a < b) ? a : b;
}

static FFINL size_t ffmin(size_t a, size_t b)
{
	return (a < b) ? a : b;
}

#define ffmax(i0, i1) \
	(((i0) < (i1)) ? (i1) : (i0))

#define ffint_setmin(dst, src) \
do { \
	if ((dst) > (src)) \
		(dst) = (src); \
} while (0)

#define ffint_setmax(dst, src) \
do { \
	if ((dst) < (src)) \
		(dst) = (src); \
} while (0)


/** Convert host integer <-> little/big endian. */

#if defined FF_BIG_ENDIAN
	#define ffhtol64(i)  ffint_bswap64(i)
	#define ffhtol32(i)  ffint_bswap32(i)
	#define ffhtol16(i)  ffint_bswap16(i)

	#define ffhton64(i)  (i)
	#define ffhton32(i)  (i)
	#define ffhton16(i)  (i)

#elif defined FF_LITTLE_ENDIAN
	#define ffhtol64(i)  (i)
	#define ffhtol32(i)  (i)
	#define ffhtol16(i)  (i)

	#define ffhton64(i)  ffint_bswap64(i)
	#define ffhton32(i)  ffint_bswap32(i)
	#define ffhton16(i)  ffint_bswap16(i)
#endif


#define ff_ispow2(n)  ((((n) - 1) & (n)) == 0)

/** Align number to lower/upper boundary. */

/** @align: must be a power of 2. */
#define ff_align_floor2(n, align)  ((n) & ~(uint64)((align) - 1))
#define ff_align_ceil2(n, align)  ff_align_floor2((n) + (align) - 1, align)

#define ff_align_floor(n, align)  ((n) / (align) * (align))
#define ff_align_ceil(n, align)  ff_align_floor((n) + (align) - 1, align)


#ifndef FF_64
	/** Safe cast 'uint64' to 'size_t'. */
	#define FF_TOSIZE(i)  (size_t)ffmin64(i, (size_t)-1)
#else
	#define FF_TOSIZE(i)  (size_t)(i)
#endif

#if defined FF_SAFECAST_SIZE_T && defined FF_64
	/** Safe cast 'size_t' to 'uint'. */
	#define FF_TOINT(i)  (uint)ffmin(i, (uint)-1)
#else
	#define FF_TOINT(i)  (uint)(i)
#endif

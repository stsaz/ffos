/** GCC functions.
Copyright (c) 2017 Simon Zolin
*/


typedef long long int64;
typedef unsigned long long uint64;

#define FF_FUNC __func__

#define FFINL  inline

#ifdef FF_MINGW
	#define FF_EXP  __declspec(dllexport)
	#define FF_IMP  __declspec(dllimport)
#else
	#define FF_EXP  __attribute__((visibility("default")))
	#define FF_IMP
#endif


/** Swap bytes in integer number.
e.g. "0x11223344" <-> "0x44332211" */
#define ffint_bswap16  __builtin_bswap16
#define ffint_bswap32  __builtin_bswap32
#define ffint_bswap64  __builtin_bswap64


#define ffbit_ffs32(i)  __builtin_ffs(i)

#ifdef FF_64
#define ffbit_ffs64(i)  __builtin_ffsll(i)
#endif

static FFINL uint ffbit_find32(uint n)
{
	return (n == 0) ? 0 : __builtin_clz(n) + 1;
}

#ifdef FF_64
static FFINL uint ffbit_find64(uint64 n)
{
	return (n == 0) ? 0 : __builtin_clzll(n) + 1;
}
#endif


/** Prevent compiler from reordering operations. */
#define ff_compiler_fence()  __asm volatile("" : : : "memory")


/** Set module constructor function. */
#define FFDL_ONINIT(init, fin) \
void _ffdl_oninit(void)__attribute__((constructor)); \
void _ffdl_oninit(void) \
{ \
	init(); \
}

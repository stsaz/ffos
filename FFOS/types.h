/**
Target architecture, OS;  base types;  base macros.
Copyright (c) 2013 Simon Zolin
*/

#ifndef FF_VER

#define FF_VER  0x010d0000

#if defined __LP64__ || defined _WIN64
	#define FF_64
#endif

#if defined __FreeBSD__
	#define FF_BSD
	#include <FFOS/unix/types.h>

#elif defined __linux__
	#define FF_LINUX
	#ifndef __cplusplus
		#define _GNU_SOURCE
	#endif
	#define _LARGEFILE64_SOURCE
	#include <FFOS/unix/types.h>

#elif defined _WIN32 || defined _WIN64 || defined __CYGWIN__
	#define FF_WIN
	#include <FFOS/win/types.h>

#else
	#error This kernel is not supported.
#endif

typedef int ffbool;

#ifdef __cplusplus
	#define FF_EXTN extern "C"
#else
	#define FF_EXTN extern
#endif

#ifdef _DEBUG
	#define FF_ASSERT(expr)  assert(expr)
#else
	#define FF_ASSERT(expr)
#endif

#ifndef FF_MSVC
	#define FF_FUNC __func__
#else
	#define FF_FUNC __FUNCTION__
#endif

#define FFOFF(structType, member)  (((size_t)&((structType *)0)->member))

#define FF_GETPTR(struct_type, member_name, member_ptr) \
	((struct_type*)((byte*)member_ptr - FFOFF(struct_type, member_name)))


#define FF_LITTLE_ENDIAN  1234
#define FF_BIG_ENDIAN  4321
#define FFENDIAN  FF_LITTLE_ENDIAN

#if FFENDIAN == FF_BIG_ENDIAN
	#define ffhton64(i)  (i)
	#define ffhton32(i)  (i)
	#define ffhton16(i)  (i)

#else
#ifdef FF_BSD
	#define ffhton64  bswap64
	#define ffhton32  bswap32
	#define ffhton16  bswap16

#elif defined FF_LINUX || defined __CYGWIN__
	#define ffhton64  bswap_64
	#define ffhton32  bswap_32
	#define ffhton16  bswap_16

#elif defined FF_MSVC || defined FF_MINGW
	#define ffhton64  _byteswap_uint64
	#define ffhton32  _byteswap_ulong
	#define ffhton16  _byteswap_ushort
#endif
#endif

#define FFCNT(ar)  (sizeof(ar) / sizeof(*(ar)))

#define FF_SAFECLOSE(obj, def, func)\
do { \
	if (obj != def) { \
		func(obj); \
		obj = def; \
	} \
} while (0)

#define ffabs(n) \
({ \
	__typeof__(n) _n = (n); \
	(_n >= 0) ? _n : -_n; \
})

static FFINL uint64 ffmin64(uint64 i0, uint64 i1) {
	return (i0 > i1 ? i1 : i0);
}

static FFINL size_t ffmin(size_t i0, size_t i1) {
	return (i0 > i1 ? i1 : i0);
}

#define ffmax(i0, i1) \
	(((i0) < (i1)) ? (i1) : (i0))

#if defined FF_SAFECAST_SIZE_T && !defined FF_64
	/** Safer cast 'uint64' to 'size_t'. */
	#define FF_TOSIZE(i)  (size_t)ffmin64(i, (size_t)-1)
#else
	#define FF_TOSIZE(i)  (size_t)(i)
#endif

#if defined FF_SAFECAST_SIZE_T && defined FF_64
	/** Safer cast 'size_t' to 'uint'. */
	#define FF_TOINT(i)  (uint)ffmin(i, (uint)-1)
#else
	#define FF_TOINT(i)  (uint)(i)
#endif

#define FF_BIT64(bit)  (1ULL << (bit))

#define FF_LO32(i64)  ((int)((i64) & 0xffffffff))

#define FF_HI32(i64)  ((int)(((i64) >> 32) & 0xffffffff))

/** Align number to upper boundary.
@align: must be a power of 2. */
#define ff_align(n, align)  (((n) & ((align) - 1)) ? ((n) & ~((align) - 1)) + (align) : (n))


#if defined FF_MSVC
#define FFDL_ONINIT(init, fin) \
BOOL DllMain(HMODULE p1, DWORD reason, void *p3) \
{ \
	if (reason == DLL_PROCESS_ATTACH) \
		init(); \
	return 1; \
}

#else
/** Set module constructor function. */
#define FFDL_ONINIT(init, fin) \
void _ffdl_oninit(void)__attribute__((constructor)); \
void _ffdl_oninit(void) \
{ \
	init(); \
}
#endif


enum FFDBG_T {
	FFDBG_MEM = 0x10,
	FFDBG_KEV = 0x20,
	FFDBG_PARSE = 0x100,
};

/** Print FF debug messages.
@t: enum FFDBG_T + level. */
extern int ffdbg_print(int t, const char *fmt, ...);

#ifdef _DEBUG
extern int ffdbg_mask; //~(enum FFDBG_T) + level
#define FFDBG_PRINT(t, ...) \
do { \
	if ((ffdbg_mask & 0x0f) >= ((t) & 0x0f) && !((ffdbg_mask & (t)) & ~0x0f)) \
		ffdbg_print(t, __VA_ARGS__); \
} while (0)

#define FFDBG_PRINTLN(t, fmt, ...) \
do { \
	if ((ffdbg_mask & 0x0f) >= ((t) & 0x0f) && !((ffdbg_mask & (t)) & ~0x0f)) \
		ffdbg_print(t, "%s(): " fmt "\n", FF_FUNC, __VA_ARGS__); \
} while (0)

#else
#define FFDBG_PRINT(...)
#define FFDBG_PRINTLN(...)
#endif

#endif //FF_VER

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
	#include <FFOS/unix/types.h>

#elif defined _WIN32 || defined _WIN64 || defined __CYGWIN__
	#ifndef FF_WIN
		#define FF_WIN 0x0600
	#endif
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


#define FF_SAFECLOSE(obj, def, func)\
do { \
	if (obj != def) { \
		func(obj); \
		obj = def; \
	} \
} while (0)


#define FF_BIT64(bit)  (1ULL << (bit))

#define FF_LO32(i64)  ((int)((i64) & 0xffffffff))

#define FF_HI32(i64)  ((int)(((i64) >> 32) & 0xffffffff))


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

#define FFDBG_CHKLEV(t) \
	((ffdbg_mask & 0x0f) >= ((t) & 0x0f) && !((ffdbg_mask & (t)) & ~0x0f))

#define FFDBG_PRINT(t, ...) \
do { \
	if (FFDBG_CHKLEV(t)) \
		ffdbg_print(t, __VA_ARGS__); \
} while (0)

#define FFDBG_PRINTLN(t, fmt, ...) \
do { \
	if (FFDBG_CHKLEV(t)) \
		ffdbg_print(t, "%s(): " fmt "\n", FF_FUNC, __VA_ARGS__); \
} while (0)

#else
#define FFDBG_PRINT(...)
#define FFDBG_PRINTLN(...)
#endif


#include <FFOS/number.h>

#endif //FF_VER

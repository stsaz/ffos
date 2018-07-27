/**
Target architecture, OS;  base types;  base macros.
Copyright (c) 2013 Simon Zolin
*/

#ifndef FF_VER

#define FF_VER  0x010d0000

#if defined __amd64__ || defined _M_AMD64
	#define FF_AMD64
#elif defined __i386__ || defined _M_IX86
	#define FF_X86
#elif defined __arm__ || defined _M_ARM
	#define FF_ARM
#elif defined __aarch64__
	#define FF_ARM
#else
	#error This CPU is not supported
#endif
#if defined __LP64__ || defined _WIN64
	#define FF_64
#endif


#if defined __linux__
	#define FF_UNIX
	#define FF_LINUX
	#ifdef ANDROID
		#define FF_ANDROID
	#else
		#define FF_LINUX_MAINLINE
	#endif
	#include <FFOS/linux/types.h>
	#include <FFOS/unix/types.h>

#elif defined __APPLE__ && defined __MACH__
	#define FF_UNIX
	#define FF_APPLE
	#include <FFOS/bsd/types.h>
	#include <FFOS/unix/types.h>

#elif defined __unix__
	#define FF_UNIX
	#include <sys/param.h>
	#if defined BSD
		#define FF_BSD
		#include <FFOS/bsd/types.h>
	#endif
	#include <FFOS/unix/types.h>

#elif defined _WIN32 || defined _WIN64 || defined __CYGWIN__
	#ifndef FF_WIN
		#define FF_WIN 0x0600
	#endif
	#include <FFOS/win/types.h>

#else
	#error This kernel is not supported.
#endif


#if defined __clang__
	#define FF_CLANG
	#include <FFOS/compiler-gcc.h>

// #elif defined _MSC_VER
// 	#define FF_MSVC

#elif defined __MINGW32__ || defined __MINGW64__
	#define FF_MINGW
	#include <FFOS/compiler-gcc.h>

#elif defined __GNUC__
	#define FF_GCC
	#include <FFOS/compiler-gcc.h>

#else
	#error "This compiler is not supported"
#endif


typedef signed char ffint8;
typedef int ffbool;

#ifdef __cplusplus
	#define FF_EXTN extern "C"
#else
	#define FF_EXTN extern
#endif

#if defined _DEBUG || defined FF_ASSERT_ENABLED
	#include <assert.h>
	#define FF_ASSERT(expr)  assert(expr)
#else
	#define FF_ASSERT(expr)
#endif


#define FF_SAFECLOSE(obj, def, func)\
do { \
	if (obj != def) { \
		func(obj); \
		obj = def; \
	} \
} while (0)


enum FFDBG_T {
	FFDBG_MEM = 0x10,
	FFDBG_KEV = 0x20,
	FFDBG_TIMER = 0x40,
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

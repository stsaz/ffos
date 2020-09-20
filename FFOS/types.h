/**
Target architecture, OS;  base types;  base macros.
Copyright (c) 2013 Simon Zolin
*/

#ifndef FF_VER

#define FF_VER  0x010d0000

#include <FFOS/detect-os.h>

#if defined FF_LINUX
	#ifndef __cplusplus
		#define _GNU_SOURCE
	#endif

	#ifndef _LARGEFILE64_SOURCE
		#define _LARGEFILE64_SOURCE 1
	#endif

	#if defined FF_OLDLIBC && defined FF_64
		__asm__(".symver memcpy,memcpy@GLIBC_2.2.5"); //override GLIBC_2.14
	#endif

	#if defined FF_GLIBCVER && FF_GLIBCVER < 229
		__asm__(".symver pow,pow@GLIBC_2.2.5"); //override GLIBC_2.29
	#endif
#endif

#if defined FF_UNIX
	#include <stdlib.h>
	#include <unistd.h>
	#include <string.h>

	enum {
		FF_BADFD = -1
	};

	typedef char ffsyschar;
	typedef int fffd;
	typedef unsigned char byte;
	typedef unsigned short ushort;
	typedef unsigned int uint;

#elif defined FF_WIN

#ifdef _MSC_VER
	#define FF_MSVC
#endif

#ifndef UNICODE
	#define UNICODE
#endif

#ifndef _UNICODE
	#define _UNICODE
#endif

#define _WIN32_WINNT FF_WIN_APIVER
#define NOMINMAX
#define _CRT_SECURE_NO_WARNINGS
#define OEMRESOURCE //gui
#include <winsock2.h>

#define FF_LITTLE_ENDIAN

#define FF_BADFD  INVALID_HANDLE_VALUE

typedef WCHAR ffsyschar;

typedef HANDLE fffd;

//byte;
typedef unsigned short ushort;
typedef unsigned int uint;

#ifdef FF_MSVC
	#define FF_EXP  __declspec(dllexport)
	#define FF_IMP  __declspec(dllimport)

	#ifndef _SIZE_T_DEFINED
		typedef SIZE_T size_t;
		#define _SIZE_T_DEFINED
	#endif
	typedef SSIZE_T ssize_t;

	#define FF_FUNC __FUNCTION__

	#define va_copy(vadst, vasrc)  vadst = vasrc

	#define ffint_bswap16  _byteswap_ushort
	#define ffint_bswap32  _byteswap_ulong
	#define ffint_bswap64  _byteswap_uint64

#define FFDL_ONINIT(init, fin) \
BOOL DllMain(HMODULE p1, DWORD reason, void *p3) \
{ \
	if (reason == DLL_PROCESS_ATTACH) \
		init(); \
	return 1; \
}

#endif

#endif

#define ffmem_alloc // ffbase won't define memory allocation functions
#include <ffbase/base.h>
#undef ffmem_alloc

#include <FFOS/detect-compiler.h>
#include <FFOS/compiler-gcc.h>

typedef signed char ffint8;
typedef int ffbool;

#ifdef __cplusplus
	#define FF_EXTN extern "C"
#else
	#define FF_EXTN extern
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

extern int ffdbg_mask; //~(enum FFDBG_T) + level

#define FFDBG_CHKLEV(t) \
	((ffdbg_mask & 0x0f) >= ((t) & 0x0f) && !((ffdbg_mask & (t)) & ~0x0f))

#ifdef FF_DEBUG
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

/**
Target architecture, OS;  base types;  base macros.
Copyright (c) 2013 Simon Zolin
*/

#ifndef _FFOS_BASE_H
#include <FFOS/base.h>
#endif

#ifndef FF_VER
#define FF_VER

#if defined FF_UNIX
	#include <stdlib.h>
	#include <unistd.h>
	#include <string.h>

	enum {
		FF_BADFD = -1
	};

	typedef unsigned char byte;
	typedef unsigned short ushort;
	typedef unsigned int uint;

#elif defined FF_WIN

#ifdef _MSC_VER
	#define FF_MSVC
#endif

#define FF_BADFD  INVALID_HANDLE_VALUE

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

#include <FFOS/error-compat.h>
#include <FFOS/file-compat.h>
#include <FFOS/mem-compat.h>
#include <FFOS/number-compat.h>
#include <FFOS/process.h>
#include <FFOS/process-compat.h>
#include <FFOS/socket.h>
#include <FFOS/socket-compat.h>
#include <FFOS/queue.h>
#include <FFOS/queue-compat.h>
#include <FFOS/thread-compat.h>
#include <FFOS/timer-compat.h>

#endif

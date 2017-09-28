/**
Copyright (c) 2013 Simon Zolin
*/

#ifdef _MSC_VER
	#define FF_MSVC
#endif

#if defined __MINGW32__ || defined __MINGW64__
	#define FF_MINGW
#endif

#ifndef UNICODE
	#define UNICODE
#endif

#ifndef _UNICODE
	#define _UNICODE
#endif

#define _WIN32_WINNT FF_WIN
#define NOMINMAX
#define _CRT_SECURE_NO_WARNINGS
#define OEMRESOURCE //gui
#include <winsock2.h>
#include <assert.h>

#ifdef __CYGWIN__
	#include <sys/types.h>
#endif

#define FF_LITTLE_ENDIAN

#ifdef FF_MSVC
	#define FFINL //msvc9 doesn't like "static inline func()"
#else
	#define FFINL  inline
#endif

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
#endif

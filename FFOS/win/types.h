/**
Copyright (c) 2013 Simon Zolin
*/

#ifdef _MSC_VER
	#define FF_MSVC
#endif

#if defined __MINGW64__
	#define FF_MINGW
#endif

#ifndef UNICODE
	#define UNICODE
#endif

#ifndef _UNICODE
	#define _UNICODE
#endif

#ifndef _WIN32_WINNT
	#define _WIN32_WINNT 0x0600
#endif

#define NOMINMAX
#define _CRT_SECURE_NO_WARNINGS
#define OEMRESOURCE //gui
#include <winsock2.h>
#include <assert.h>

#ifdef __CYGWIN__
	#include <sys/types.h>
#endif

#ifdef FF_MSVC
	#define FFINL //msvc9 doesn't like "static inline func()"
#else
	#define FFINL  inline
#endif

#define FF_EXP  __declspec(dllexport)
#define FF_IMP  __declspec(dllimport)

#define FF_BADFD  INVALID_HANDLE_VALUE

typedef WCHAR ffsyschar;

typedef HANDLE fffd;

//byte;
typedef unsigned short ushort;
typedef unsigned int uint;

#ifdef FF_MSVC
	typedef __int64 int64;
	typedef unsigned __int64 uint64;
#elif defined FF_MINGW
	typedef long long int64;
	typedef unsigned long long uint64;
#else
	typedef int64_t int64;
	typedef uint64_t uint64;
#endif

#ifdef FF_MSVC
	#ifndef _SIZE_T_DEFINED
		typedef SIZE_T size_t;
		#define _SIZE_T_DEFINED
	#endif
	typedef SSIZE_T ssize_t;
#endif

#ifdef FF_MSVC
	#define va_copy(vadst, vasrc)  vadst = vasrc
#endif

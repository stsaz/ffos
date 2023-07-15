/** ffos: detect CPU, OS and compiler; base types; heap memory functions
2020, Simon Zolin
*/

#ifndef _FFOS_BASE_H
#define _FFOS_BASE_H

#include <FFOS/detect-os.h>

#if defined FF_LINUX
	#ifndef _GNU_SOURCE
		#define _GNU_SOURCE
	#endif

	#ifndef _LARGEFILE64_SOURCE
		#define _LARGEFILE64_SOURCE 1
	#endif
#endif

#if defined FF_UNIX
	#include <stdlib.h>
	#include <unistd.h>
	#include <string.h>
	#include <errno.h>

	typedef int fffd;
	typedef char ffsyschar;

#else // Windows:

	#define _WIN32_WINNT FF_WIN_APIVER
	#define NOMINMAX
	#define _CRT_SECURE_NO_WARNINGS
	#define OEMRESOURCE //gui
	#include <winsock2.h>

	typedef HANDLE fffd;
	typedef wchar_t ffsyschar;

#endif

#include <ffbase/base.h>

#include <FFOS/detect-compiler.h>
#include <FFOS/compiler-gcc.h>

typedef signed char ffint8;
typedef int ffbool;

#endif

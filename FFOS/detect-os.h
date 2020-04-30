/** Preprocessor detection of OS.
Copyright (c) 2020 Simon Zolin

OS family types:
. FF_UNIX
. FF_WIN

OS types:
. FF_LINUX (mainline Linux + android)
. FF_LINUX_MAINLINE (mainline Linux)
. FF_ANDROID (Linux android)
. FF_APPLE (OS X)
. FF_BSD
. FF_WIN
*/


#if defined __linux__
	#define FF_UNIX
	#define FF_LINUX
	#ifdef ANDROID
		#define FF_ANDROID
	#else
		#define FF_LINUX_MAINLINE
	#endif

#elif defined __APPLE__ && defined __MACH__
	#define FF_UNIX
	#define FF_APPLE

#elif defined __unix__
	#define FF_UNIX
	#include <sys/param.h>
	#if defined BSD
		#define FF_BSD
	#endif

#elif defined _WIN32 || defined _WIN64 || defined __CYGWIN__
	#ifndef FF_WIN
		#define FF_WIN 0x0600
	#endif

#else
	#error "This OS is not supported"
#endif

/**
Copyright (c) 2017 Simon Zolin
*/

#ifndef __cplusplus
	#define _GNU_SOURCE
#endif
#define _LARGEFILE64_SOURCE


#if defined FF_OLDLIBC && defined FF_64
__asm__(".symver memcpy,memcpy@GLIBC_2.2.5"); //override GLIBC_2.14
#endif


#include <asm/byteorder.h>
#if defined __LITTLE_ENDIAN
	#define FF_LITTLE_ENDIAN
#elif defined __BIG_ENDIAN
	#define FF_BIG_ENDIAN
#else
	#error Undefined endian
#endif


typedef unsigned short ushort;
typedef unsigned int uint;

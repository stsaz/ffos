/**
Copyright (c) 2013 Simon Zolin
*/

#define FF_UNIX

#ifdef FF_LINUX
#if defined FF_OLDLIBC && defined FF_64
__asm__(".symver memcpy,memcpy@GLIBC_2.2.5"); //override GLIBC_2.14
#endif

#include <byteswap.h>

#elif defined FF_BSD
#include <sys/endian.h>

#endif

#include <unistd.h>
#include <stdint.h>
#include <assert.h>

#define FFINL  inline

#define FF_EXP  __attribute__((visibility("default")))

enum {
	FF_BADFD = -1
};

typedef char ffsyschar;
typedef int fffd;

typedef unsigned char byte;
#ifdef FF_LINUX
	typedef unsigned short ushort;
	typedef unsigned int uint;
#endif
typedef int64_t int64;
typedef uint64_t uint64;
typedef ssize_t ssize_t;
typedef size_t size_t;

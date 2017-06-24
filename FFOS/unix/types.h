/**
Copyright (c) 2013 Simon Zolin
*/

#define FF_UNIX

#ifdef FF_LINUX
	#include <FFOS/linux/types.h>
#else
	#include <FFOS/bsd/types.h>
#endif

#include <unistd.h>
#include <stdint.h>
#include <assert.h>

#define FFINL  inline

#define FF_EXP  __attribute__((visibility("default")))
#define FF_IMP

enum {
	FF_BADFD = -1
};

typedef char ffsyschar;
typedef int fffd;

typedef unsigned char byte;
typedef int64_t int64;
typedef uint64_t uint64;
typedef ssize_t ssize_t;
typedef size_t size_t;

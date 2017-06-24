/**
Copyright (c) 2017 Simon Zolin
*/

#pragma once

#include <sys/endian.h>


#if _BYTE_ORDER == _LITTLE_ENDIAN
	#define FF_LITTLE_ENDIAN
#elif _BYTE_ORDER == _BIG_ENDIAN
	#define FF_BIG_ENDIAN
#else
	#error Undefined endian
#endif

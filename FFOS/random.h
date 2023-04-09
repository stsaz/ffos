/** ffos: random number generator
2020, Simon Zolin
*/

#pragma once

#include <FFOS/base.h>

#ifdef FF_WIN

static inline void ffrand_seed(ffuint seed)
{
	(void)seed;
}

static inline int ffrand_get()
{
	ffuint i;
	rand_s(&i);
	return i;
}

#else

static inline void ffrand_seed(ffuint seed)
{
	srandom(seed);
}

static inline int ffrand_get()
{
	return random();
}

#endif


/** Initialize random number generator */
static void ffrand_seed(ffuint seed);

/** Get random number */
static int ffrand_get();

/** ffos: random number generator
2020, Simon Zolin
*/

#pragma once

#include <FFOS/base.h>

#ifdef FF_WIN

static inline void ffrand_seed(ffuint seed)
{
	srand(seed);
}

static inline int ffrand_get()
{
	return rand();
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


#ifndef FFOS_NO_COMPAT
#define ffrnd_seed  ffrand_seed
#define ffrnd_get  ffrand_get
#endif

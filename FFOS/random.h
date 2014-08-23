/** Random number generator.
Copyright (c) 2014 Simon Zolin
*/

#pragma once

#ifdef FF_UNIX

/** Initialize random number generator. */
#define ffrnd_seed  srandom

/** Get random number. */
#define ffrnd_get  random

#else //FF_WIN:

#define ffrnd_seed  srand
#define ffrnd_get  rand

#endif

/** Semaphore.
Copyright (c) 2018 Simon Zolin
*/

#ifdef FF_UNIX
#include <FFOS/unix/sem.h>
#elif defined FF_WIN
#include <FFOS/win/sem.h>
#endif

/** Decrease semaphore value, blocking if necessary.
time_ms: max time to wait;  0:try;  -1:wait forever */
FF_EXTN int ffsem_wait(ffsem s, uint time_ms);

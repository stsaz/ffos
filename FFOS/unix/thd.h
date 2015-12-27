/**
Copyright (c) 2013 Simon Zolin
*/

#if !defined FF_NOTHR
#include <pthread.h>


#define FFTHDCALL

typedef int (FFTHDCALL *ffthdproc)(void *);

typedef pthread_t ffthd;

#define FFTHD_INV 0

/** Create a thread.
@stack_size: 0=default.
Return 0 on error. */
FF_EXTN ffthd ffthd_create(ffthdproc proc, void *param, size_t stack_size);

/** Join with the thread.
Return 0 if thread has exited.
Return ETIMEDOUT if time has expired. */
FF_EXTN int ffthd_join(ffthd th, uint timeout_ms, int *exit_code);

/** Detach thread. */
#define ffthd_detach  pthread_detach

#endif //FF_NOTHR

FF_EXTN void ffthd_sleep(uint ms);

/**
Copyright (c) 2013 Simon Zolin
*/

#define FFTHDCALL __stdcall

typedef int (FFTHDCALL *ffthdproc)(void *);

typedef HANDLE ffthd;
typedef uint ffthd_id;

#define FFTHD_INV NULL

#define ffthd_create(proc, param, stack_size)  ffthread_create(proc, param, stack_size)
#define ffthd_join(th, timeout_ms, exit_code)  ffthread_join(th, timeout_ms, exit_code)
#define ffthd_detach(th)  ffthread_detach(th)
#define ffthd_sleep(ms)  ffthread_sleep(ms)
#define ffthd_curid()  ffthread_curid()

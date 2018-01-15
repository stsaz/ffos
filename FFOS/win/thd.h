/**
Copyright (c) 2013 Simon Zolin
*/

#define FFTHDCALL __stdcall

typedef int (FFTHDCALL *ffthdproc)(void *);

typedef HANDLE ffthd;
typedef uint ffthd_id;

#define FFTHD_INV NULL

static FFINL ffthd ffthd_create(ffthdproc proc, void *param, size_t stack_size) {
	return CreateThread(NULL, stack_size, (PTHREAD_START_ROUTINE)proc, param, 0, NULL);
}

FF_EXTN int ffthd_join(ffthd th, uint timeout_ms, int *exit_code);

static FFINL int ffthd_detach(ffthd th) {
	return 0 == CloseHandle(th);
}

#define ffthd_sleep  Sleep

#define ffthd_curid()  GetCurrentThreadId()

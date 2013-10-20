/**
Copyright (c) 2013 Simon Zolin
*/

#define FFTHDCALL __stdcall

typedef int (FFTHDCALL *ffthdproc)(void *);

typedef HANDLE ffthd;

static FFINL ffthd ffthd_create(ffthdproc proc, void *param, size_t stack_size) {
	return CreateThread(NULL, stack_size, (PTHREAD_START_ROUTINE)proc, param, 0, NULL);
}

FF_EXTN int ffthd_join(ffthd th, uint timeout_ms, int *exit_code);

#define ffthd_detach(th)  (0 == CloseHandle(th))

#define ffthd_sleep  Sleep

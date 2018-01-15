/**
Copyright (c) 2017 Simon Zolin
*/

#include <FFOS/thread.h>

#include <errno.h>
#if !defined FF_NOTHR && defined FF_BSD
#include <pthread_np.h>
#endif


#if !defined FF_NOTHR
ffthd ffthd_create(ffthdproc proc, void *param, size_t stack_size)
{
	ffthd th = 0;
	typedef void* (*start_routine) (void *);
	pthread_attr_t attr;
	pthread_attr_t *pattr = NULL;

	if (stack_size != 0) {
		if (0 != pthread_attr_init(&attr))
			return 0;
		pattr = &attr;
		if (0 != pthread_attr_setstacksize(&attr, stack_size))
			goto end;
	}

	if (0 != pthread_create(&th, pattr, (start_routine)proc, param))
		th = 0;

end:
	if (pattr != NULL)
		pthread_attr_destroy(pattr);

	return th;
}

#if defined FF_APPLE
int ffthd_join(ffthd th, uint timeout_ms, int *exit_code)
{
	int r;
	void *result;

	if (timeout_ms == (uint)-1) {
		r = pthread_join(th, &result);
		if (r != 0)
			return r;
	} else {
		return ETIMEDOUT;
	}

	if (exit_code != NULL)
		*exit_code = (int)(size_t)result;
	return 0;
}

#else
extern void fftimespec_addms(struct timespec *t, uint64 ms);

int ffthd_join(ffthd th, uint timeout_ms, int *exit_code)
{
	void *result;
	int r;

	if (timeout_ms == (uint)-1) {
		r = pthread_join(th, &result);
	}

#if defined FF_LINUX
	else if (timeout_ms == 0) {
		r = pthread_tryjoin_np(th, &result);
		if (r == EBUSY)
			r = ETIMEDOUT;
	}
#endif

	else {
		struct timespec ts;
		(void)clock_gettime(CLOCK_REALTIME, &ts);
		fftimespec_addms(&ts, timeout_ms);
		r = pthread_timedjoin_np(th, &result, &ts);
	}

	if (r != 0)
		return r;

	if (exit_code != NULL)
		*exit_code = (int)(size_t)result;
	return 0;
}
#endif //OS

#if defined FF_APPLE
ffthd_id ffthd_curid(void)
{
	uint64_t tid;
	if (0 != pthread_threadid_np(NULL, &tid))
		return 0;
	return tid;
}

#elif defined FF_BSD
ffthd_id ffthd_curid(void)
{
	return pthread_getthreadid_np();
}
#endif //OS

#endif //FF_NOTHR
/**
Copyright (c) 2017 Simon Zolin
*/

#include <FFOS/thread.h>
#include <FFOS/semaphore.h>
#include <FFOS/error.h>

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

	if (0 != pthread_create(&th, pattr, (start_routine)(void*)proc, param))
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

#if defined FF_LINUX_MAINLINE
	else if (timeout_ms == 0) {
		r = pthread_tryjoin_np(th, &result);
		if (r == EBUSY)
			r = ETIMEDOUT;
	}
#endif

#if defined FF_LINUX_MAINLINE || defined FF_BSD
	else {
		struct timespec ts;
		(void)clock_gettime(CLOCK_REALTIME, &ts);
		fftimespec_addms(&ts, timeout_ms);
		r = pthread_timedjoin_np(th, &result, &ts);
	}
#else
	else {
		errno = EINVAL;
		return -1;
	}
#endif

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


int ffsem_wait(ffsem sem, uint time_ms)
{
	if (sem == FFSEM_INV) {
		fferr_set(EINVAL);
		return -1;
	}
	sem_t *s = (sem->psem != NULL) ? sem->psem : &sem->sem;

	int r;
	if (time_ms == 0) {
		if (0 != (r = sem_trywait(s)) && fferr_again(fferr_last()))
			fferr_set(ETIMEDOUT);

	} else if (time_ms == (uint)-1) {
		do {
			r = sem_wait(s);
		} while (r != 0 && fferr_last() == EINTR);

	} else {
#ifdef FF_APPLE
		fferr_set(EINVAL);
		return -1;
#else
		struct timespec ts = {};
		clock_gettime(CLOCK_REALTIME, &ts);
		ts.tv_sec += time_ms / 1000;
		ts.tv_nsec += (time_ms % 1000) * 1000 * 1000;
		do {
			r = sem_timedwait(s, &ts);
		} while (r != 0 && fferr_last() == EINTR);
#endif
	}
	return r;
}

#endif //FF_NOTHR

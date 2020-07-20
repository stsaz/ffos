/**
Copyright (c) 2017 Simon Zolin
*/

#include <FFOS/thread.h>
#include <FFOS/semaphore.h>
#include <FFOS/error.h>


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

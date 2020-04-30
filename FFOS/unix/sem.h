/** UNIX semaphore.
Copyright (c) 2018 Simon Zolin
*/

#include <FFOS/mem.h>
#include <FFOS/error.h>
#include <semaphore.h>


struct _ffsem {
	sem_t *psem; // pointer to a named semaphore
	sem_t sem; // data for a unnamed semaphore.  NOT allocated for named semaphore!
};
typedef struct _ffsem* ffsem;

#define FFSEM_INV  NULL

/** Open or create a semaphore.
name: "/semname"
 NULL: create an unnamed semaphore
flags: 0 | FFO_CREATE | FFO_CREATENEW
 Note: FFO_CREATENEW: Windows: not atomic.
value: initial value
Return FFSEM_INV on error. */
static inline ffsem ffsem_open(const char *name, uint flags, uint value)
{
	struct _ffsem *s;
	if (name == NULL) {
		if (NULL == (s = ffmem_allocT(1, struct _ffsem)))
			return FFSEM_INV;
		if (0 != sem_init(&s->sem, 0, value)) {
			ffmem_free(s);
			return FFSEM_INV;
		}
		s->psem = NULL;
		return s;
	}

	if (NULL == (s = ffmem_alloc(sizeof(void*))))
		return FFSEM_INV;
	if (SEM_FAILED == (s->psem = sem_open(name, flags, 0666, value))) {
		ffmem_free(s);
		return FFSEM_INV;
	}
	return s;
}

/** Close semaphore. */
static inline void ffsem_close(ffsem s)
{
	if (s == FFSEM_INV)
		return;

	if (s->psem != NULL)
		sem_close(s->psem);
	else
		sem_destroy(&s->sem);
	ffmem_free(s);
}

/** Delete semaphore.
UNIX: semaphore is destroyed on system shutdown.
Windows: semaphore is destroyed when the last reference is closed. */
#define ffsem_unlink(name)  sem_unlink(name)

/** Increase semaphore value. */
static inline int ffsem_post(ffsem s)
{
	if (s == FFSEM_INV) {
		fferr_set(EINVAL);
		return -1;
	}

	sem_t *p = (s->psem != NULL) ? s->psem : &s->sem;
	return sem_post(p);
}

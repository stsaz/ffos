/** UNIX semaphore.
Copyright (c) 2018 Simon Zolin
*/

#include <FFOS/types.h>
#include <semaphore.h>


typedef sem_t* ffsem;

#define FFSEM_INV  SEM_FAILED

/** Open or create a semaphore.
name: "/semname"
flags: 0 | FFO_CREATE | FFO_CREATENEW
 Note: FFO_CREATENEW: Windows: not atomic.
value: initial value
Return FFSEM_INV on error. */
static inline ffsem ffsem_open(const char *name, uint flags, uint value)
{
	return sem_open(name, flags, 0666, value);
}

/** Close semaphore. */
#define ffsem_close(s)  sem_close(s)

/** Delete semaphore.
UNIX: semaphore is destroyed on system shutdown.
Windows: semaphore is destroyed when the last reference is closed. */
#define ffsem_unlink(name)  sem_unlink(name)

/** Increase semaphore value. */
#define ffsem_post(s)  sem_post(s)

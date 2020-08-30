/** Semaphore
2020, Simon Zolin
*/

/*
ffsem_open
ffsem_close
ffsem_wait
ffsem_unlink
ffsem_post
*/

#include <FFOS/types.h>

#ifdef FF_WIN

#include <FFOS/string.h>

typedef HANDLE ffsem;
#define FFSEM_NULL  NULL

enum FFSEM_OPEN {
	FFSEM_CREATE = 1,
	FFSEM_CREATENEW = 2,
};

static inline ffsem ffsem_open(const char *name, ffuint flags, ffuint value)
{
	if (name == NULL)
		return CreateSemaphore(NULL, value, 0xffff, NULL);

	wchar_t wbuf[256], *wname = NULL;
	if (NULL == (wname = ffsz_alloc_buf_utow(wbuf, FF_COUNT(wbuf), name)))
		return FFSEM_NULL;

	ffsem s = FFSEM_NULL;
	if (flags == FFSEM_CREATENEW) {
		s = OpenSemaphore(SEMAPHORE_ALL_ACCESS, 0, wname);
		if (s != FFSEM_NULL) {
			CloseHandle(s);
			SetLastError(ERROR_ALREADY_EXISTS);
			s = FFSEM_NULL;
		} else {
			s = CreateSemaphore(NULL, value, 0xffff, wname);
		}

	} else if (flags == FFSEM_CREATE) {
		s = CreateSemaphore(NULL, value, 0xffff, wname);

	} else if (flags == 0) {
		s = OpenSemaphore(SEMAPHORE_ALL_ACCESS, 0, wname);

	} else {
		SetLastError(ERROR_INVALID_PARAMETER);
	}

	if (wname != wbuf)
		ffmem_free(wname);
	return s;
}

static inline void ffsem_close(ffsem s)
{
	CloseHandle(s);
}

static inline int ffsem_wait(ffsem s, ffuint time_ms)
{
	int r = WaitForSingleObject(s, time_ms);
	if (r == WAIT_OBJECT_0)
		r = 0;
	else if (r == WAIT_TIMEOUT)
		SetLastError(WSAETIMEDOUT);
	return r;
}

static inline int ffsem_unlink(const char *name)
{
	(void)name;
	return 0;
}

static inline int ffsem_post(ffsem s)
{
	return !ReleaseSemaphore(s, 1, NULL);
}

#elif defined FF_APPLE

#include <FFOS/mem.h>
#include <FFOS/error.h>
#include <ffbase/string.h>
#include <semaphore.h>
#include <fcntl.h>
#include <time.h>

typedef sem_t* ffsem;
#define FFSEM_NULL  NULL

enum FFSEM_OPEN {
	FFSEM_CREATE = O_CREAT,
	FFSEM_CREATENEW = O_CREAT | O_EXCL,
};

static inline ffsem ffsem_open(const char *name, ffuint flags, ffuint value)
{
	char *aname = NULL;
	if (name == NULL) {
		if (NULL == (aname = ffsz_allocfmt("/tmp/ffsem-%u", getpid())))
			return FFSEM_NULL;
		name = aname;
		flags = O_CREAT | O_EXCL;
	}

	sem_t *sem;
	if (SEM_FAILED == (sem = sem_open(name, flags, 0666, value))) {
		sem = NULL;
		goto end;
	}

	if (aname != NULL)
		sem_unlink(aname);

end:
	if (aname != NULL)
		ffmem_free(aname);
	return sem;
}

static inline void ffsem_close(ffsem sem)
{
	if (sem == FFSEM_NULL)
		return;

	sem_close(sem);
}

static inline int ffsem_wait(ffsem sem, ffuint time_ms)
{
	if (sem == FFSEM_NULL) {
		errno = EINVAL;
		return -1;
	}

	int r;
	if (time_ms == 0) {
		if (0 != (r = sem_trywait(sem)) && fferr_again(errno))
			errno = ETIMEDOUT;

	} else if (time_ms == (ffuint)-1) {
		do {
			r = sem_wait(sem);
		} while (r != 0 && errno == EINTR);

	} else {
		errno = EINVAL;
		return -1;
	}
	return r;
}

static inline int ffsem_unlink(const char *name)
{
	return sem_unlink(name);
}

static inline int ffsem_post(ffsem sem)
{
	if (sem == FFSEM_NULL) {
		errno = EINVAL;
		return -1;
	}

	return sem_post(sem);
}

#else // Linux/BSD:

#include <FFOS/mem.h>
#include <FFOS/error.h>
#include <semaphore.h>
#include <fcntl.h>
#include <time.h>

struct _ffsem {
	sem_t *psem; // pointer to a named semaphore
	sem_t sem; // data for an unnamed semaphore.  NOT allocated for named semaphore!
};
typedef struct _ffsem* ffsem;
#define FFSEM_NULL  NULL

enum FFSEM_OPEN {
	FFSEM_CREATE = O_CREAT,
	FFSEM_CREATENEW = O_CREAT | O_EXCL,
};

static inline ffsem ffsem_open(const char *name, ffuint flags, ffuint value)
{
	struct _ffsem *sem;
	if (name == NULL) {
		if (NULL == (sem = ffmem_new(struct _ffsem)))
			return FFSEM_NULL;
		if (0 != sem_init(&sem->sem, 0, value)) {
			ffmem_free(sem);
			return FFSEM_NULL;
		}
		sem->psem = NULL;
		return sem;
	}

	if (NULL == (sem = (struct _ffsem*)ffmem_alloc(sizeof(void*))))
		return FFSEM_NULL;
	if (SEM_FAILED == (sem->psem = sem_open(name, flags, 0666, value))) {
		ffmem_free(sem);
		return FFSEM_NULL;
	}
	return sem;
}

static inline void ffsem_close(ffsem sem)
{
	if (sem == FFSEM_NULL)
		return;

	if (sem->psem != NULL)
		sem_close(sem->psem);
	else
		sem_destroy(&sem->sem);
	ffmem_free(sem);
}

static inline int ffsem_wait(ffsem sem, ffuint time_ms)
{
	if (sem == FFSEM_NULL) {
		errno = EINVAL;
		return -1;
	}
	sem_t *s = (sem->psem != NULL) ? sem->psem : &sem->sem;

	int r;
	if (time_ms == 0) {
		if (0 != (r = sem_trywait(s)) && fferr_again(errno))
			errno = ETIMEDOUT;

	} else if (time_ms == (ffuint)-1) {
		do {
			r = sem_wait(s);
		} while (r != 0 && errno == EINTR);

	} else {
		struct timespec ts = {};
		clock_gettime(CLOCK_REALTIME, &ts);
		ts.tv_sec += time_ms / 1000;
		ts.tv_nsec += (time_ms % 1000) * 1000 * 1000;
		do {
			r = sem_timedwait(s, &ts);
		} while (r != 0 && errno == EINTR);
	}
	return r;
}

static inline int ffsem_unlink(const char *name)
{
	return sem_unlink(name);
}

static inline int ffsem_post(ffsem sem)
{
	if (sem == FFSEM_NULL) {
		errno = EINVAL;
		return -1;
	}

	sem_t *s = (sem->psem != NULL) ? sem->psem : &sem->sem;
	return sem_post(s);
}

#endif


/** Open or create a semaphore
name: "/semname"
  NULL: create an unnamed semaphore
flags: 0 | FFSEM_CREATE | FFSEM_CREATENEW
  Note: FFSEM_CREATENEW: Windows: not atomic
value: initial value
Return FFSEM_NULL on error */
static ffsem ffsem_open(const char *name, ffuint flags, ffuint value);

/** Close semaphore */
static void ffsem_close(ffsem s);

/** Decrease semaphore value, blocking if necessary
UNIX: EINTR is handled automatically
time_ms: max time to wait
  0: try
 -1: wait forever
 macOS: only 0 or -1 values are supported */
static int ffsem_wait(ffsem s, ffuint time_ms);

/** Delete semaphore
UNIX: semaphore is destroyed on system shutdown
Windows: semaphore is destroyed when the last reference is closed */
static int ffsem_unlink(const char *name);

/** Increase semaphore value */
static int ffsem_post(ffsem s);


#define FFSEM_INV  FFSEM_NULL

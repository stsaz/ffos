/** Windows semaphore.
Copyright (c) 2018 Simon Zolin
*/

#include <FFOS/types.h>


typedef HANDLE ffsem;

#define FFSEM_INV  NULL

FF_EXTN ffsem ffsem_openq(const ffsyschar *name, uint flags, uint value);
FF_EXTN ffsem ffsem_open(const char *name, uint flags, uint value);

static inline int ffsem_close(ffsem s)
{
	return !CloseHandle(s);
}

static inline int ffsem_unlink(const char *name)
{
	return 0;
}

#define ffsem_post(s)  !ReleaseSemaphore(s, 1, NULL)

FF_EXTN int ffsem_wait(ffsem s, uint time_ms);

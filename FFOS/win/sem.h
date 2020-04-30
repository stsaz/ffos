/** Windows semaphore.
Copyright (c) 2018 Simon Zolin
*/

#include <FFOS/types.h>


typedef HANDLE ffsem;

#define FFSEM_INV  NULL

FF_EXTN ffsem ffsem_openq(const ffsyschar *name, uint flags, uint value);
FF_EXTN ffsem ffsem_open(const char *name, uint flags, uint value);

static inline void ffsem_close(ffsem s)
{
	CloseHandle(s);
}

static inline int ffsem_unlink(const char *name)
{
	return 0;
}

static inline int ffsem_post(ffsem s)
{
	return !ReleaseSemaphore(s, 1, NULL);
}

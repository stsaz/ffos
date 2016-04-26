/** System registry.
Copyright (c) 2016 Simon Zolin
*/

#include <FFOS/types.h>


typedef HKEY ffwreg;
#define FFWREG_BADKEY  ((HKEY)-1)

/** Open registry key.
@hk: HKEY_*
@flags: KEY_READ | KEY_ALL_ACCESS
*/
FF_EXTN ffwreg ffwreg_open(HKEY hk, const char *path, uint flags);

#define ffwreg_close(k)  RegCloseKey(k)

/**
Return the number of bytes read. */
FF_EXTN int ffwreg_readstr(ffwreg k, const char *name, char *d, size_t *cap);

#if 1
FF_EXTN int ffwreg_readbuf(ffwreg k, const char *name, ffarr *buf);
#endif

/**
Return 0 on success. */
FF_EXTN int ffwreg_writestr(ffwreg k, const char *name, const char *val, size_t len);

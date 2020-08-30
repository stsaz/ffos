/** System registry.
Copyright (c) 2016 Simon Zolin
*/

#include <FFOS/file.h>

typedef HKEY ffwinreg;
#define FFWINREG_NULL  ((HKEY)-1)

#define ffwreg  ffwinreg
#define FFWREG_BADKEY  FFWINREG_NULL
#define ffwreg_close  ffwinreg_close

/* Mask:
.......f  Creation
......f0  Access */
enum FFWINREG_OPEN {
	// Creation:
	FFWINREG_CREATE = 1,
	FFWINREG_CREATENEW = 2,

	// Access:
	FFWINREG_READONLY = GENERIC_READ >> 24,
	FFWINREG_WRITEONLY = GENERIC_WRITE >> 24,
	FFWINREG_READWRITE = (GENERIC_READ | GENERIC_WRITE) >> 24,
};

/** Open or create registry key
hk: HKEY_CLASSES_ROOT || HKEY_CURRENT_USER || HKEY_LOCAL_MACHINE || HKEY_USERS
flags: enum FFWINREG_OPEN
Return FFWINREG_NULL on error */
FF_EXTN ffwinreg ffwinreg_open(HKEY hk, const char *path, ffuint flags);

/** Close key */
static inline void ffwinreg_close(ffwinreg k)
{
	RegCloseKey(k);
}

struct ffwreg_info {
	DWORD subkeys;
	DWORD values;
	DWORD max_subkey_len;
	DWORD max_valname_len;
	DWORD max_val_len;
	FILETIME mtime;
};

FF_EXTN int ffwreg_info(ffwreg k, struct ffwreg_info *info);

static FFINL int ffwreg_isstr(int type)
{
	return (type == REG_SZ || type == REG_EXPAND_SZ);
}

typedef struct ffwreg_val {
	uint type; //REG_*
	char *data;
	size_t datalen;
} ffwreg_val;

/** Read value.
If val->data == NULL, a new buffer is allocated.
Return 1 on success;  0 if more data is needed (val->datalen contains value's size);  -1 on error. */
FF_EXTN int ffwreg_read(ffwreg k, const char *name, ffwreg_val *val);

/** Write value.
Return 0 on success. */
FF_EXTN int ffwreg_write(ffwreg k, const char *name, ffwreg_val *val);

static FFINL int ffwreg_writestr(ffwreg k, const char *name, const char *val, size_t len)
{
	ffwreg_val v;
	v.type = REG_SZ;
	v.data = (char*)val;
	v.datalen = len;
	return ffwreg_write(k, name, &v);
}

static FFINL int ffwreg_writeint(ffwreg k, const char *name, uint n)
{
	ffwreg_val v;
	v.type = REG_DWORD;
	v.data = (char*)&n;
	v.datalen = sizeof(int);
	return ffwreg_write(k, name, &v);
}

/** Delete value or subkey.
@name: if NULL, delete subkey.
Return 0 on success. */
FF_EXTN int ffwreg_del(ffwreg k, const char *subkey, const char *name);

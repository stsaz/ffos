/** ffos: Windows registry
2020, Simon Zolin
*/

/*
ffwinreg_open
ffwinreg_close
ffwinreg_info ffwinreg_isstr
ffwinreg_read
ffwinreg_write ffwinreg_writestr ffwinreg_writeint
ffwinreg_del
*/

#include <FFOS/string.h>

typedef HKEY ffwinreg;
#define FFWINREG_NULL  ((HKEY)-1)

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
static inline ffwinreg ffwinreg_open(HKEY hk, const char *path, ffuint flags)
{
	int r;
	wchar_t ws[255], *w;
	ffwinreg k = FFWINREG_NULL, kk;
	ffuint opts, mode = (flags & 0x0000000f);

	if (NULL == (w = ffsz_alloc_buf_utow(ws, FF_COUNT(ws), path)))
		return FFWINREG_NULL;

	if ((flags & FFWINREG_READWRITE) == FFWINREG_READWRITE)
		opts = KEY_ALL_ACCESS;
	else if (flags & FFWINREG_WRITEONLY)
		opts = KEY_WRITE;
	else
		opts = KEY_READ;

	if (mode == FFWINREG_CREATE || mode == FFWINREG_CREATENEW) {
		DWORD res;
		if (0 != (r = RegCreateKeyExW(hk, w, 0, NULL, 0, opts, NULL, &kk, &res))) {
			SetLastError(r);
			goto end;
		}
		if (mode == FFWINREG_CREATENEW && res != REG_CREATED_NEW_KEY) {
			SetLastError(ERROR_ALREADY_EXISTS);
			goto end;
		}
	} else {
		if (0 != (r = RegOpenKeyExW(hk, w, 0, opts, &kk))) {
			SetLastError(r);
			goto end;
		}
	}

	k = kk;

end:
	if (w != ws)
		ffmem_free(w);
	return k;
}

/** Close key */
static inline void ffwinreg_close(ffwinreg k)
{
	RegCloseKey(k);
}

struct ffwinreg_info {
	DWORD subkeys;
	DWORD values;
	DWORD max_subkey_len;
	DWORD max_valname_len;
	DWORD max_val_len;
	FILETIME mtime;
};

static inline int ffwinreg_info(ffwinreg k, struct ffwinreg_info *nf)
{
	int r;
	if (0 != (r = RegQueryInfoKeyW(k, NULL, NULL, NULL
		, &nf->subkeys, &nf->max_subkey_len, /*lpcMaxClassLen*/ NULL
		, &nf->values, &nf->max_valname_len, &nf->max_val_len
		, /*lpcbSecurityDescriptor*/ NULL, &nf->mtime))) {
		SetLastError(r);
		return -1;
	}
	return 0;
}

static inline int ffwinreg_isstr(int type)
{
	return (type == REG_SZ || type == REG_EXPAND_SZ);
}

typedef struct ffwinreg_val {
	ffuint type; //REG_*
	char *data;
	ffsize datalen;
} ffwinreg_val;

/** Read value
If val->data == NULL, a new buffer is allocated
Return 1 on success
  0 if more data is needed (val->datalen contains value's size)
  -1 on error

Read value from Registry:
. Get value type, data size, data itself (if small enough, into a stack buffer) from Registry
* If value's type is text:
  . If data is large, allocate temporary buffer, then re-read from Registry
  . Allocate buffer for UTF-8 data, if necessary
  . Convert to UTF-8
* If value's type isn't text:
  . Allocate buffer for value data, if necessary
  . If data is large, then re-read from Registry
  . or copy data from stack buffer
*/
static inline int ffwinreg_read(ffwinreg k, const char *name, ffwinreg_val *val)
{
	int r = -1;
	ffsize n;
	DWORD type, type2, size;
	wchar_t wname_s[256], *wname = NULL;
	char val_s[256], *valbuf = NULL;
	void *dataptr = val->data;

	n = FF_COUNT(wname_s);
	if (NULL == (wname = ffs_utow(wname_s, &n, name, -1)))
		goto end;

	for (;;) {

		size = sizeof(val_s);
		r = RegQueryValueExW(k, wname, NULL, &type, (ffbyte*)val_s, &size);

		if (r == 0 || r == ERROR_MORE_DATA) {
		} else {
			r = -1;
			goto end;
		}

		if (type == REG_SZ || type == REG_EXPAND_SZ) {

			valbuf = val_s;
			if (r == ERROR_MORE_DATA) {
				if (NULL == (valbuf = (char*)ffmem_alloc(size))) {
					r = -1;
					goto end;
				}

				r = RegQueryValueExW(k, wname, NULL, &type2, (ffbyte*)valbuf, &size);

				if (r == 0) {

				} else if (r == ERROR_MORE_DATA) {
					// value has just become larger
					ffmem_free(valbuf);
					valbuf = NULL;
					continue;

				} else {
					r = -1;
					goto end;
				}

				if (type2 != type) {
					// value type has just been changed
					ffmem_free(valbuf);
					valbuf = NULL;
					continue;
				}
			}

			if (((wchar_t*)valbuf)[size / sizeof(wchar_t)] == '\0')
				size--;

			if (val->data == NULL) {
				r = ffs_wtou(NULL, 0, (wchar_t*)valbuf, size / sizeof(wchar_t));
				if (r < 0) {
					SetLastError(EINVAL);
					r = -1;
					goto end;
				}
				if (NULL == (val->data = (char*)ffmem_alloc(r))) {
					r = -1;
					goto end;
				}
				val->datalen = r;
			}

			r = ffs_wtou(val->data, val->datalen, (wchar_t*)valbuf, size / sizeof(wchar_t));
			if (r < 0) {
				// buffer supplied by user is too small
				val->datalen = size / sizeof(wchar_t) * 4;
				r = 0;
				goto end;
			}
			val->datalen = r;

		} else { //not text:

			if (val->data == NULL) {
				if (NULL == (val->data = (char*)ffmem_alloc(size))) {
					r = -1;
					goto end;
				}
				val->datalen = size;

			} else {
				if (val->datalen < size) {
					// buffer supplied by user is too small
					val->datalen = size;
					r = 0;
					goto end;
				}
			}

			if (r == ERROR_MORE_DATA) {

				size = val->datalen;
				r = RegQueryValueExW(k, wname, NULL, &type2, (ffbyte*)val->data, &size);

				if (r == 0) {

				} else if (r == ERROR_MORE_DATA) {
					// value has just become larger
					if (val->data != dataptr) {
						ffmem_free(val->data);
						val->data = NULL;
					}
					continue;

				} else {
					r = -1;
					goto end;
				}

				if (type2 != type) {
					// value type has just been changed
					if (val->data != dataptr) {
						ffmem_free(val->data);
						val->data = NULL;
					}
					continue;
				}

				val->datalen = size;

			} else {
				ffmem_copy(val->data, val_s, size);
				val->datalen = size;
			}
		}

		break;

	} //for()

	val->type = type;
	r = 1;

end:
	if (wname != wname_s)
		ffmem_free(wname);
	if (valbuf != val_s)
		ffmem_free(valbuf);
	if (r != 1 && val->data != dataptr) {
		ffmem_free(val->data);
		val->data = NULL;
	}
	return r;
}

/** Write value
Return 0 on success */
static inline int ffwinreg_write(ffwinreg k, const char *name, ffwinreg_val *val)
{
	int r = -1;
	wchar_t wname_s[256], *wname = NULL;
	wchar_t wval_s[256], *wval = NULL;
	ffstr d;

	if (NULL == (wname = ffsz_alloc_buf_utow(wname_s, FF_COUNT(wname_s), name))) {
		r = -1;
		goto end;
	}

	ffstr_set(&d, val->data, val->datalen);
	if (val->type == REG_SZ || val->type == REG_EXPAND_SZ) {
		ffsize n = FF_COUNT(wval_s);
		if (NULL == (wval = ffs_alloc_buf_utow(wval_s, &n, val->data, val->datalen))) {
			r = -1;
			goto end;
		}
		wval[n++] = '\0';
		ffstr_set(&d, (void*)wval, n * sizeof(wchar_t));
	}

	r = RegSetValueExW(k, wname, 0, val->type, (ffbyte*)d.ptr, d.len);
	if (r != 0) {
		r = -1;
		goto end;
	}

	r = 0;

end:
	if (wname != wname_s)
		ffmem_free(wname);
	if (wval != wval_s)
		ffmem_free(wval);
	return r;
}

static inline int ffwinreg_writestr(ffwinreg k, const char *name, const char *val, ffsize len)
{
	ffwinreg_val v;
	v.type = REG_SZ;
	v.data = (char*)val;
	v.datalen = len;
	return ffwinreg_write(k, name, &v);
}

static inline int ffwinreg_writeint(ffwinreg k, const char *name, ffuint n)
{
	ffwinreg_val v;
	v.type = REG_DWORD;
	v.data = (char*)&n;
	v.datalen = sizeof(int);
	return ffwinreg_write(k, name, &v);
}

/** Delete value or subkey
name: if NULL, delete subkey
Return 0 on success */
static inline int ffwinreg_del(ffwinreg k, const char *subkey, const char *name)
{
	int r = -1;
	wchar_t wname_s[256], *wname = NULL;
	wchar_t wsubkey_s[256], *wsubkey = NULL;

	if (NULL == (wsubkey = ffsz_alloc_buf_utow(wsubkey_s, FF_COUNT(wsubkey_s), subkey)))
		goto end;

	if (name != NULL) {
#if FF_WIN >= 0x0600
		if (NULL == (wname = ffsz_alloc_buf_utow(wname_s, FF_COUNT(wname_s), name)))
			goto end;
		r = RegDeleteKeyValueW(k, wsubkey, wname);
#else
		SetLastError(ERROR_INVALID_PARAMETER);
		r = -1;
#endif

	} else {
		r = RegDeleteKeyW(k, wsubkey);
	}

end:
	if (wsubkey != wsubkey_s)
		ffmem_free(wsubkey);
	if (wname != wname_s)
		ffmem_free(wname);
	return r;
}


#ifndef FFOS_NO_COMPAT
#define ffwreg  ffwinreg
#define ffwreg_val  ffwinreg_val
#define ffwreg_info  ffwinreg_info
#define FFWREG_BADKEY  FFWINREG_NULL
#define ffwreg_open  ffwinreg_open
#define ffwreg_close  ffwinreg_close
#define ffwreg_isstr  ffwinreg_isstr
#define ffwreg_write  ffwinreg_write
#define ffwreg_writestr  ffwinreg_writestr
#define ffwreg_writeint  ffwinreg_writeint
#define ffwreg_del  ffwinreg_del
#define ffwreg_read  ffwinreg_read
#endif

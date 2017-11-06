/**
Copyright (c) 2017 Simon Zolin
*/

#include <FFOS/mem.h>
#include <FFOS/win/reg.h>
#include <FFOS/string.h>
#include <FFOS/error.h>

#ifndef FF_FFOS_ONLY
extern size_t ffutf8_from_utf16le(char *dst, size_t cap, const char *src, size_t *len, uint flags);
#endif


ffwreg ffwreg_open(HKEY hk, const char *path, uint flags)
{
	int r;
	ffsyschar ws[255], *w;
	ffwreg k = FFWREG_BADKEY, kk;
	size_t n = FFCNT(ws);
	uint opts, mode = (flags & 0x0000000f);

	if (NULL == (w = ffs_utow(ws, &n, path, -1)))
		return FFWREG_BADKEY;

	if ((flags & O_RDWR) == O_RDWR)
		opts = KEY_ALL_ACCESS;
	else if (flags & O_WRONLY)
		opts = KEY_WRITE;
	else
		opts = KEY_READ;

	if (mode == O_CREAT || mode == FFO_CREATENEW) {
		DWORD res;
		if (0 != (r = RegCreateKeyExW(hk, w, 0, NULL, 0, opts, NULL, &kk, &res))) {
			fferr_set(r);
			goto end;
		}
		if (mode == FFO_CREATENEW && res != REG_CREATED_NEW_KEY) {
			fferr_set(ERROR_ALREADY_EXISTS);
			goto end;
		}
	} else {
		if (0 != (r = RegOpenKeyEx(hk, w, 0, opts, &kk))) {
			fferr_set(r);
			goto end;
		}
	}

	k = kk;

end:
	if (w != ws)
		ffmem_free(w);
	return k;
}

int ffwreg_info(ffwreg k, struct ffwreg_info *nf)
{
	int r;
	if (0 != (r = RegQueryInfoKey(k, NULL, NULL, NULL
		, &nf->subkeys, &nf->max_subkey_len , /*lpcMaxClassLen*/ NULL
		, &nf->values, &nf->max_valname_len, &nf->max_val_len
		, /*lpcbSecurityDescriptor*/ NULL, &nf->mtime))) {
		fferr_set(r);
		return -1;
	}
	return 0;
}


/* Read value from Registry:
. Get value type, data size, data itself (if small enough, into stack buffer) from Registry
. If value's type is text:
  . If data is large, allocate temporary buffer, then re-get from Registry
  . Allocate buffer for UTF-8 data, if necessary
  . Convert to UTF-8
. If value's type isn't text:
  . Allocate buffer for value data, if necessary
  . If data is large, then re-get from Registry
  . or copy data from stack buffer
*/
int ffwreg_read(ffwreg k, const char *name, ffwreg_val *val)
{
	int r = -1;
	size_t n;
	DWORD type, type2, size;
	ffsyschar wname_s[256], *wname = NULL;
	char val_s[256], *valbuf = NULL;
	void *dataptr = val->data;

	n = FFCNT(wname_s);
	if (NULL == (wname = ffs_utow(wname_s, &n, name, -1)))
		goto end;

	for (;;) {

		size = sizeof(val_s);
		r = RegQueryValueEx(k, wname, NULL, &type, (void*)val_s, &size);

		if (r == 0 || r == ERROR_MORE_DATA) {

		} else {
			if (r == ERROR_FILE_NOT_FOUND)
				fferr_set(ENOENT);
			r = -1;
			goto end;
		}

		if (type == REG_SZ || type == REG_EXPAND_SZ) {

			valbuf = val_s;
			if (r == ERROR_MORE_DATA) {
				if (NULL == (valbuf = ffmem_alloc(size))) {
					r = -1;
					goto end;
				}

				r = RegQueryValueEx(k, wname, NULL, &type2, (void*)valbuf, &size);

				if (r == 0) {

				} else if (r == ERROR_MORE_DATA) {
					// value has just become larger
					ffmem_free0(valbuf);
					continue;

				} else {
					if (r == ERROR_FILE_NOT_FOUND)
						fferr_set(ENOENT);
					r = -1;
					goto end;
				}

				if (type2 != type) {
					// value type has just been changed
					ffmem_free0(valbuf);
					continue;
				}
			}

			if (((ffsyschar*)valbuf)[size / sizeof(ffsyschar)] == '\0')
				size--;

			if (val->data == NULL) {
				n = size;
#ifdef FF_FFOS_ONLY
				r = n * 4;
#else
				r = ffutf8_from_utf16le(NULL, 0, valbuf, &n, 0);
#endif

				if (NULL == (val->data = ffmem_alloc(r))) {
					r = -1;
					goto end;
				}
				val->datalen = r;
			}

			r = ff_wtou(val->data, val->datalen, (void*)valbuf, size / sizeof(ffsyschar), 0);
			val->datalen = r;
			if (r == 0 && size != 0) {
				// buffer supplied by user is too small
				val->datalen = size / sizeof(ffsyschar) * 4;
				r = 0;
				goto end;
			}

		} else { //not text:

			if (val->data == NULL) {
				if (NULL == (val->data = ffmem_alloc(size))) {
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
				r = RegQueryValueEx(k, wname, NULL, &type2, (void*)val->data, &size);

				if (r == 0) {

				} else if (r == ERROR_MORE_DATA) {
					// value has just become larger
					if (val->data != dataptr)
						ffmem_free0(val->data);
					continue;

				} else {
					if (r == ERROR_FILE_NOT_FOUND)
						fferr_set(ENOENT);
					r = -1;
					goto end;
				}

				if (type2 != type) {
					// value type has just been changed
					if (val->data != dataptr)
						ffmem_free0(val->data);
					continue;
				}

				val->datalen = size;

			} else {
				memcpy(val->data, val_s, size);
				val->datalen = size;
			}
		}

		break;

	} //for()

	val->type = type;
	r = 1;

end:
	if (wname != wname_s)
		ffmem_safefree(wname);
	if (valbuf != val_s)
		ffmem_safefree(valbuf);
	if (r != 1 && val->data != dataptr)
		ffmem_free0(val->data);
	return r;
}

int ffwreg_write(ffwreg k, const char *name, ffwreg_val *val)
{
	int r = -1;
	size_t n;
	ffsyschar wname_s[256], *wname = NULL;
	ffsyschar wval_s[256], *wval = NULL;
	char *data;
	size_t datalen;

	n = FFCNT(wname_s);
	if (NULL == (wname = ffs_utow(wname_s, &n, name, -1))) {
		r = -1;
		goto end;
	}

	data = val->data;
	datalen = val->datalen;
	if (val->type == REG_SZ || val->type == REG_EXPAND_SZ) {
		n = FFCNT(wval_s);
		if (NULL == (wval = ffs_utow(wval_s, &n, val->data, val->datalen))) {
			r = -1;
			goto end;
		}
		wval[n++] = '\0';
		data = (void*)wval;
		datalen = n * sizeof(ffsyschar);
	}

	r = RegSetValueEx(k, wname, 0, val->type, (void*)data, datalen);
	if (r != 0) {
		r = -1;
		goto end;
	}

	r = 0;

end:
	if (wname != wname_s)
		ffmem_safefree(wname);
	if (wval != wval_s)
		ffmem_safefree(wval);
	return r;
}

int ffwreg_del(ffwreg k, const char *subkey, const char *name)
{
	int r = -1;
	size_t n;
	ffsyschar wname_s[256], *wname = NULL;
	ffsyschar wsubkey_s[256], *wsubkey = NULL;

	n = FFCNT(wsubkey_s);
	if (NULL == (wsubkey = ffs_utow(wsubkey_s, &n, subkey, -1)))
		goto end;

	if (name != NULL) {
#if FF_WIN >= 0x0600
		n = FFCNT(wname_s);
		if (NULL == (wname = ffs_utow(wname_s, &n, name, -1)))
			goto end;
		r = RegDeleteKeyValue(k, wsubkey, wname);
#else
		fferr_set(EINVAL);
		r = -1;
#endif

	} else {
		r = RegDeleteKey(k, wsubkey);
	}

end:
	if (wsubkey != wsubkey_s)
		ffmem_safefree(wsubkey);
	if (wname != wname_s)
		ffmem_safefree(wname);
	return r;
}

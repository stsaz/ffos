/**
Copyright (c) 2017 Simon Zolin
*/

#include <FFOS/mem.h>
#include <FFOS/win/reg.h>
#include <FFOS/win/str.h>
#include <FFOS/win/error.h>


ffwreg ffwreg_open(HKEY hk, const char *path, uint flags)
{
	ffsyschar ws[255], *w;
	ffwreg k = FFWREG_BADKEY;
	size_t n = FFCNT(ws);
	if (NULL == (w = ffs_utow(ws, &n, path, -1)))
		return FFWREG_BADKEY;

	RegCreateKeyExW(hk, w, 0, NULL, 0, flags, NULL, &k, NULL);

	if (w != ws)
		ffmem_free(w);
	return k;
}

int ffwreg_readstr(ffwreg k, const char *name, char *d, size_t *cap)
{
	int r = -1;
	ffsyschar ws[255], *w;
	ffsyschar *wval = NULL;
	DWORD t, size;
	size_t n = FFCNT(ws);
	if (NULL == (w = ffs_utow(ws, &n, name, -1)))
		return -1;

	for (;;) {
		t = REG_SZ;
		size = 0;
		if (RegQueryValueExW(k, w, 0, &t, NULL, &size))
			goto done;

		if ((size - 1) / sizeof(ffsyschar) * 4 > *cap) {
			*cap = (size - 1) / sizeof(ffsyschar) * 4;
			fferr_set(ERROR_MORE_DATA);
			goto done;
		}

		if (NULL == (wval = ffmem_alloc(size)))
			goto done;

		t = REG_SZ;
		if (RegQueryValueExW(k, w, 0, &t, (void*)wval, &size)) {
			if (fferr_last() == ERROR_MORE_DATA) {
				ffmem_free(wval);
				wval = NULL;
				continue;
			}
			goto done;
		}

		break;
	}

	if (!(t == REG_SZ || t == REG_EXPAND_SZ)) {
		fferr_set(EINVAL);
		goto done;
	}

	if (size != 0)
		size = (size - 1) / sizeof(ffsyschar);

	r = ff_wtou(d, *cap, wval, size, 0);

done:
	ffmem_safefree(wval);
	if (w != ws)
		ffmem_free(w);
	return r;
}

int ffwreg_writestr(ffwreg k, const char *name, const char *val, size_t len)
{
	int r = -1;
	ffsyschar ws[255], *w;
	ffsyschar ws_val[255], *wval;
	size_t n = FFCNT(ws);
	if (NULL == (w = ffs_utow(ws, &n, name, -1)))
		return -1;

	n = FFCNT(ws_val);
	if (NULL == (wval = ffs_utow(ws_val, &n, val, len)))
		goto done;
	wval[n++] = '\0';

	uint type = REG_SZ;
	if (!RegSetValueExW(k, w, 0, type, (void*)wval, n * sizeof(ffsyschar)))
		r = 0;

done:
	if (wval != ws_val)
		ffmem_safefree(wval);
	if (w != ws)
		ffmem_safefree(w);
	return r;
}

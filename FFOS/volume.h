/** ffos: volumes (Windows)
2023, Simon Zolin
*/

/*
ffvol_open ffvol_close
ffvol_next
ffvol_info ffvol_info_destroy
ffvol_mount
*/

#ifdef FF_WIN

#include <FFOS/string.h>
#include <ffbase/vector.h>

/**
Return FFFILE_NULL on error */
static inline fffd ffvol_open(wchar_t *buf, ffsize cap)
{
	return FindFirstVolumeW(buf, cap);
}

static inline void ffvol_close(fffd h)
{
	FindVolumeClose(h);
}

#define FFERR_NOMOREVOLS  ERROR_NO_MORE_FILES

/**
Return !=0 with FFERR_NOMOREVOLS if no more entries */
static inline int ffvol_next(fffd h, wchar_t *buf, ffsize cap)
{
	if (!FindNextVolumeW(h, buf, cap))
		return 1;
	return 0;
}

struct ffvol_info {
	ffuint type;
	DWORD sectors_cluster, bytes_sector, clusters_free, clusters_total;
	wchar_t fs[64];
	ffvec paths; // NULL-terminated 2d array: wchar_t*[]
};

static inline void ffvol_info_destroy(struct ffvol_info *vi)
{
	ffvec_free(&vi->paths);
}

/**
Return !=0 on error */
static inline int ffvol_info(const wchar_t *name, struct ffvol_info *vi, ffuint flags)
{
	int r = 0;
	if (flags == 0)
		flags = 0xff;

	vi->type = GetDriveTypeW(name);

	if (flags & 1) {
		if (!GetDiskFreeSpaceW(name, &vi->sectors_cluster, &vi->bytes_sector, &vi->clusters_free, &vi->clusters_total))
			r |= 1;
	}

	if (flags & 2) {
		if (!GetVolumeInformationW(name, NULL, 0, NULL, NULL, NULL, vi->fs, FF_COUNT(vi->fs)))
			r |= 2;
	}

	if (flags & 4) {
		DWORD size = MAX_PATH + 1;
		for (;;) {
			ffvec_allocT(&vi->paths, size, wchar_t);
			if (!GetVolumePathNamesForVolumeNameW(name, (wchar_t*)vi->paths.ptr, vi->paths.cap, &size)) {
				if (GetLastError() == ERROR_MORE_DATA) {
					ffvec_free(&vi->paths);
					continue;
				}
				r |= 4;
			}
			break;
		}
	}

	return r;
}

/** Create a system string and add a backslash to the end */
static wchar_t* _ffs_utow_bslash(const char *s)
{
	ffsize n = 2;
	wchar_t *w;
	if (NULL == (w = (wchar_t*)ffs_alloc_utow_addcap(s, ffsz_len(s), &n)))
		return NULL;

	// "name" -> "name\\\0"
	if (n == 0 || s[n - 1] != '\\')
		w[n++] = '\\';
	w[n] = '\0';
	return w;
}

/** Create or delete a mount point for disk
When deleting, the mount point directory isn't deleted
disk: volume GUID name: "\\?\Volume{GUID}\"
  NULL: delete mount point
mount: mount point (an existing empty directory) */
static inline int ffvol_mount(const char *disk, const char *mount)
{
	int r = -1;
	wchar_t *wdisk = NULL, *wmount = NULL;
	if (NULL == (wmount = _ffs_utow_bslash(mount)))
		goto end;

	if (disk == NULL) {
		if (!DeleteVolumeMountPointW(wmount))
			goto end;

	} else {
		if (NULL == (wdisk = ffsz_alloc_utow(disk)))
			goto end;
		if (!SetVolumeMountPointW(wmount, wdisk))
			goto end;
	}

	r = 0;

end:
	ffmem_free(wdisk);
	ffmem_free(wmount);
	return r;
}

#endif

/** ffos: directory scan
2021, Simon Zolin
*/

/*
ffdirscan_open ffdirscan_close
ffdirscan_next
ffdirscan_reset
*/

#pragma once

#include <FFOS/dir.h>
#include <FFOS/path.h>
#include <ffbase/vector.h>
#include <ffbase/stringz.h>

typedef struct ffdirscan {
	void *names; // NAME\0... uint...
	ffsize len, index, cur;

	const char *wildcard;
	fffd fd;
} ffdirscan;

enum FFDIRSCAN_F {
	FFDIRSCAN_NOSORT = 1, // don't sort
	FFDIRSCAN_DOT = 2, // include "." and ".."

	/** Filter names by wildcard */
	FFDIRSCAN_USEWILDCARD = 0x10,

	/** Linux: use fd supplied by user.
	Don't use this fd afterwards! */
	FFDIRSCAN_USEFD = 0x20,
};

static int _ffdirscan_cmpname(const void *a, const void *b, void *udata)
{
	const char *buf = udata;
	ffuint off1 = *(ffuint*)a, off2 = *(ffuint*)b;
	if (FFPATH_ICASE)
		return ffsz_icmp(&buf[off1], &buf[off2]);
	int r = ffsz_cmp(&buf[off1], &buf[off2]);
	return r;
}

/**
flags: enum FFDIRSCAN_F
Default behaviour: skip "." and ".." entries, sort
Return 0 on success */
static inline int ffdirscan_open(ffdirscan *d, const char *path, ffuint flags)
{
	ffvec offsets = {}, buf = {};
	int rc = -1;

	ffuint wcflags;
	ffsize wclen;
	if (flags & FFDIRSCAN_USEWILDCARD) {
		wclen = ffsz_len(d->wildcard);
		wcflags = FFPATH_ICASE ? FFS_WC_ICASE : 0;
	}

#ifdef FF_WIN

	HANDLE dir = INVALID_HANDLE_VALUE;
	WIN32_FIND_DATAW ent;
	wchar_t *wpath = NULL;
	char *namebuf = NULL;
	int first = 1;

	// "/dir" -> "/dir\\*"
	ffsize n = 3;
	if (flags & FFDIRSCAN_USEWILDCARD)
		n = 2+wclen;
	if (NULL == (wpath = ffs_alloc_utow_addcap(path, ffsz_len(path), &n)))
		goto end;
	if (wpath[n-1] == '/' || wpath[n-1] == '\\')
		n--; // "dir/" -> "dir"
	wpath[n++] = '\\';
	if (flags & FFDIRSCAN_USEWILDCARD)
		n += _ffs_utow(&wpath[n], wclen, d->wildcard, wclen);
	else
		wpath[n++] = '*';
	wpath[n] = '\0';

	if (flags & FFDIRSCAN_USEFD)
		SetLastError(ERROR_NOT_SUPPORTED);
	else {
		dir = FindFirstFileW(wpath, &ent);
		if (dir == INVALID_HANDLE_VALUE && GetLastError() != ERROR_FILE_NOT_FOUND)
			goto end;
	}
	ffmem_free(wpath);  wpath = NULL;

	if (NULL == (namebuf = ffmem_alloc(256*4)))
		goto end;

#else

	DIR *dir = NULL;
	if (flags & FFDIRSCAN_USEFD) {
		dir = fdopendir(d->fd);
		if (dir == NULL)
			close(d->fd);
		d->fd = -1;
	} else {
		dir = opendir(path);
	}

	if (dir == NULL)
		goto end;
#endif

	if (NULL == ffvec_alloc(&buf, 4096, 1))
		goto end;

	for (;;) {

		const char *name;

#ifdef FF_WIN

		if (first) {
			first = 0;
			if (dir == INVALID_HANDLE_VALUE)
				break;

		} else {
			if (0 == FindNextFileW(dir, &ent)) {
				if (GetLastError() != ERROR_NO_MORE_FILES)
					goto end;
				break;
			}
		}

		if (ffsz_wtou(namebuf, 256*4, ent.cFileName) < 0) {
			SetLastError(ERROR_INVALID_DATA);
			goto end;
		}
		name = namebuf;

#else

		const struct dirent *de;
		errno = 0;
		if (NULL == (de = readdir(dir))) {
			if (errno != 0)
				goto end;
			break;
		}
		name = de->d_name;
#endif

		ffsize namelen = ffsz_len(name);

		if (!(flags & FFDIRSCAN_DOT)) {
			if (name[0] == '.'
				&& (name[1] == '\0'
					|| (name[1] == '.' && name[2] == '\0')))
				continue;
		}

		if (flags & FFDIRSCAN_USEWILDCARD) {
			if (0 != ffs_wildcard(d->wildcard, wclen, name, namelen, wcflags))
				continue;
		}

		ffuint *p = ffvec_pushT(&offsets, ffuint);
		if (p == NULL)
			goto end;
		*p = buf.len;

		if (0 == ffvec_add(&buf, name, namelen+1, 1))
			goto end;
	}

	if (!(flags & FFDIRSCAN_NOSORT))
		ffsort(offsets.ptr, offsets.len, sizeof(ffuint), _ffdirscan_cmpname, buf.ptr);

	d->index = buf.len;
	if (offsets.len != 0 && 0 == ffvec_add(&buf, offsets.ptr, offsets.len * sizeof(ffuint), 1))
		goto end;
	d->len = buf.len;
	d->cur = d->index;
	d->names = buf.ptr;
	ffvec_null(&buf);
	rc = 0;

end:
#ifdef FF_WIN
	if (dir != INVALID_HANDLE_VALUE)
		FindClose(dir);
	ffmem_free(wpath);
	ffmem_free(namebuf);
#else
	if (dir != NULL)
		closedir(dir);
#endif
	ffvec_free(&offsets);
	ffvec_free(&buf);
	return rc;
}

static inline void ffdirscan_close(ffdirscan *d)
{
	ffmem_free(d->names);  d->names = NULL;
}

/**
Return NULL if no more entries */
static inline const char* ffdirscan_next(ffdirscan *d)
{
	if (d->cur == d->len)
		return NULL;

	ffsize off = *(ffuint*)&((char*)d->names)[d->cur];
	d->cur += sizeof(ffuint);
	const char *name = &((char*)d->names)[off];
	return name;
}

static inline void ffdirscan_reset(ffdirscan *d)
{
	d->cur = d->index;
}

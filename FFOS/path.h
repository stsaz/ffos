/** ffos: file-system path
2020, Simon Zolin
*/

/*
FFDIR_USER_CONFIG FFPATH_ICASE FFPATH_SLASH
ffpath_slash
ffpath_abs
ffpath_islong
ffpath_infoinit
ffpath_info
ffpath_splitpath ffpath_splitpath_str ffpath_splitpath_win ffpath_splitpath_unix
ffpath_splitname
ffpath_normalize
*/

#pragma once

#include <FFOS/string.h>

enum FFPATH_INFO {
	FFPATH_BLOCKSIZE,
};

#ifdef FF_WIN

#define FFDIR_USER_CONFIG  "%APPDATA%"

/** Case-insensitive file system */
#define FFPATH_ICASE  1

#define FFPATH_SLASH  '\\'
#define FFPATH_SLASHES  "\\/"

static inline ffbool ffpath_slash(int c)
{
	return c == '/' || c == '\\';
}

/* [/\\] || [a-zA-Z]:[\0/\\] */
static inline ffbool ffpath_abs(const char *path, ffsize len)
{
	if (len == 0)
		return 0;

	if (path[0] == '/' || path[0] == '\\')
		return 1;

	int ch = path[0] | 0x20;
	return len >= FFS_LEN("c:")
		&& ch >= 'a' && ch <= 'z'
		&& path[1] == ':'
		&& (len == FFS_LEN("c:") || path[2] == '/' || path[2] == '\\');
}

/**
Return TRUE if long name */
static inline ffbool ffpath_islong(const wchar_t *fnw)
{
	wchar_t fnlong[4096];
	ffuint n = GetLongPathNameW(fnw, fnlong, FF_COUNT(fnlong));
	if (n == 0)
		return 0;
	if (0 != ffq_icmpz(fnlong, fnw))
		return 0;
	return 1;
}


typedef struct ffpathinfo {
	ffuint f_bsize;
} ffpathinfo;

static inline int ffpath_infoinit(const char *path, ffpathinfo *st)
{
	wchar_t *w, ws[256];
	if (NULL == (w = ffsz_alloc_buf_utow(ws, FF_COUNT(ws), path)))
		return 1;

	DWORD spc, bps, fc, c;
	BOOL b = GetDiskFreeSpaceW(w, &spc, &bps, &fc, &c);

	if (w != ws)
		ffmem_free(w);
	if (!b)
		return 1;

	st->f_bsize = spc * bps;
	return 0;
}

static inline ffuint64 ffpath_info(ffpathinfo *st, ffuint name)
{
	switch (name) {
	case FFPATH_BLOCKSIZE:
		return st->f_bsize;
	}

	return 0;
}

#define ffpath_splitpath ffpath_splitpath_win

#else // UNIX:

#if defined FF_BSD || defined FF_APPLE
	#include <sys/mount.h>
#else
	#include <sys/vfs.h>
#endif

#define FFDIR_USER_CONFIG  "$HOME/.config"

/** Case-sensitive file system */
#define FFPATH_ICASE  0

/** File system native path separator */
#define FFPATH_SLASH  '/'
#define FFPATH_SLASHES  "/"

static inline ffbool ffpath_slash(int c)
{
	return c == '/';
}

static inline ffbool ffpath_abs(const char *path, ffsize len)
{
	return len >= 1 && path[0] == '/';
}


typedef struct statfs ffpathinfo;

static inline int ffpath_infoinit(const char *path, ffpathinfo *st)
{
	return statfs(path, st);
}

static inline ffuint64 ffpath_info(ffpathinfo *st, ffuint name)
{
	switch (name) {
	case FFPATH_BLOCKSIZE:
		return st->f_bsize;
	}

	return 0;
}

#define ffpath_splitpath ffpath_splitpath_unix

#endif


/** Return TRUE if character is a valid path separator */
static ffbool ffpath_slash(int c);

/** Check whether the specified path is absolute */
static ffbool ffpath_abs(const char *path, ffsize len);


/** Initialize statfs structure */
static int ffpath_infoinit(const char *path, ffpathinfo *st);

/** Get file-system parameter
name: enum FFPATH_INFO */
static ffuint64 ffpath_info(ffpathinfo *st, ffuint name);

/** Split into path (without the last slash) and name so that "foo" is a name without path */
static inline ffssize ffpath_splitpath_unix(const char *fn, ffsize len, ffstr *dir, ffstr *name)
{
	ffssize slash = ffs_rfindchar(fn, len, '/');
	if (slash < 0) {
		if (dir != NULL)
			ffstr_null(dir);
		if (name != NULL)
			ffstr_set(name, fn, len);
		return -1;
	}
	return ffs_split(fn, len, slash, dir, name);
}

static inline ffssize ffpath_splitpath_win(const char *fn, ffsize len, ffstr *dir, ffstr *name)
{
	ffssize slash = ffs_rfindany(fn, len, "/\\", 2);
	if (slash < 0) {
		if (dir != NULL)
			ffstr_null(dir);
		if (name != NULL)
			ffstr_set(name, fn, len);
		return -1;
	}
	return ffs_split(fn, len, slash, dir, name);
}

/** Split into name and extension so that ".foo" is a name without extension */
static inline ffssize ffpath_splitname(const char *fn, ffsize len, ffstr *name, ffstr *ext)
{
	ffssize dot = ffs_rfindchar(fn, len, '.');
	if (dot <= 0) {
		if (name != NULL)
			ffstr_set(name, fn, len);
		if (ext != NULL)
			ext->len = 0;
		return 0;
	}
	return ffs_split(fn, len, dot, name, ext);
}

static inline ffssize ffpath_splitpath_str(ffstr fn, ffstr *dir, ffstr *name)
{
	return ffpath_splitpath(fn.ptr, fn.len, dir, name);
}

enum FFPATH_NORM {
	FFPATH_SLASH_ONLY = 1, // split path by slash (default on UNIX)
	FFPATH_SLASH_BACKSLASH = 2, // split path by both slash and backslash (default on Windows)
	FFPATH_FORCE_SLASH = 4, // convert '\\' to '/'
	FFPATH_FORCE_BACKSLASH = 8, // convert '/' to '\\'

	FFPATH_SIMPLE = 0x10, // convert to a simple path: {/abc, ./abc, ../abc} -> abc

	/* Handle disk drive letter, e.g. 'C:\' (default on Windows)
	C:/../a -> C:/a
	C:/a -> a (FFPATH_SIMPLE) */
	FFPATH_DISK_LETTER = 0x20,
	FFPATH_NO_DISK_LETTER = 0x40, // disable auto FFPATH_DISK_LETTER on Windows
};

/** Normalize file path
flags: enum FFPATH_NORM
Default behaviour:
  * split path by slash (on UNIX), by both slash and backslash (on Windows)
    override by FFPATH_SLASH_BACKSLASH or FFPATH_SLASH_ONLY
  * handle disk drive letter on Windows, e.g. 'C:\'
    override by FFPATH_DISK_LETTER or FFPATH_NO_DISK_LETTER
  * skip "." components, unless leading
    ./a/b -> ./a/b
    a/./b -> a/b
  * handle ".." components
    a/../b -> b
    ../a/b -> ../a/b
    ./../a -> ../a
    /../a -> /a
Return N of bytes written
  <0 on error */
static inline ffssize ffpath_normalize(char *dst, ffsize cap, const char *src, ffsize len, ffuint flags)
{
	ffsize k = 0;
	ffstr s, part;
	ffstr_set(&s, src, len);
	int simplify = !!(flags & FFPATH_SIMPLE);

	const char *slashes = (flags & FFPATH_SLASH_BACKSLASH) ? "/\\" : "/";
#ifdef FF_WIN
	if ((flags & (FFPATH_SLASH_BACKSLASH | FFPATH_SLASH_ONLY)) == 0)
		slashes = "/\\";

	if ((flags & (FFPATH_DISK_LETTER | FFPATH_NO_DISK_LETTER)) == 0)
		flags |= FFPATH_DISK_LETTER;
#endif
	int skip_disk = (flags & (FFPATH_DISK_LETTER | FFPATH_SIMPLE)) == (FFPATH_DISK_LETTER | FFPATH_SIMPLE);

	while (s.len != 0) {
		const char *s2 = s.ptr;
		ffssize pos = ffstr_splitbyany(&s, slashes, &part, &s);

		if (simplify) {
			if (part.len == 0
				|| ffstr_eqcz(&part, "."))
				continue; // merge slashes, skip dots

			if (skip_disk) {
				skip_disk = 0;
				if (*ffstr_last(&part) == ':')
					continue; // skip 'c:/'
			}

		} else {
			// allow leading slash or dot
			simplify = 1;
		}

		if (ffstr_eqcz(&part, "..")) {
			if (k != 0) {
				ffssize slash = ffs_rfindany(dst, k - 1, slashes, ffsz_len(slashes));
				ffstr prev;
				if (slash < 0) {
					ffstr_set(&prev, dst, k - 1);
					if (ffstr_eqcz(&prev, ".")) {
						k = 0; // "./" -> "../"
					} else if (ffstr_eqcz(&prev, "..")) {
						// "../" -> "../../"
					} else if (k == 1) {
						continue; // "/" -> "/"
					} else if ((flags & FFPATH_DISK_LETTER)
						&& *ffstr_last(&prev) == ':') {
						continue; // "c:/" -> "c:/"
					} else {
						k = 0; // "a/" -> ""
						continue;
					}

				} else {
					slash++;
					ffstr_set(&prev, &dst[slash], k - 1 - slash);
					if (ffstr_eqcz(&prev, "..")) {
						// ".../../" -> ".../../../"
					} else {
						k = slash; // ".../a/" -> ".../"
						continue;
					}
				}

			} else if (flags & FFPATH_SIMPLE) {
				continue;
			}
		}

		if (k + part.len + ((pos >= 0) ? 1 : 0) > cap)
			return -1;
		ffmem_copy(&dst[k], part.ptr, part.len);
		k += part.len;

		if (pos >= 0) {
			if (flags & FFPATH_FORCE_SLASH)
				dst[k] = '/';
			else if (flags & FFPATH_FORCE_BACKSLASH)
				dst[k] = '\\';
			else
				dst[k] = s2[pos];
			k++;
		}
	}

	return k;
}

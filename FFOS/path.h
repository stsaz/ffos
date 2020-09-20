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
*/

#pragma once

#include <FFOS/base.h>

enum FFPATH_INFO {
	FFPATH_BLOCKSIZE,
};

#ifdef FF_WIN

#include <FFOS/string.h>

#define FFDIR_USER_CONFIG  "%APPDATA%"

/** Case-insensitive file system */
#define FFPATH_ICASE  1

#define FFPATH_SLASH  '\\'

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

#else // UNIX:

#include <string.h>
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


#ifndef FFOS_NO_COMPAT
#define FFPATH_BSIZE  FFPATH_BLOCKSIZE
#endif

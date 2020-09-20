/** ffos: directory, path
2020, Simon Zolin
*/

/*
ffdir_make ffdir_make_all
ffdir_remove
Directory listing:
	ffdir_open ffdir_close
	ffdir_read
	ffdir_entry_name
	ffdir_entry_info
*/

#pragma once

#include <FFOS/string.h>

#ifdef FF_WIN

#define FFERR_DIREXISTS  ERROR_ALREADY_EXISTS
#define FFERR_NOMOREFILES  ERROR_NO_MORE_FILES

static inline int ffdir_make(const char *name)
{
	wchar_t ws[256], *w;
	if (NULL == (w = ffsz_alloc_buf_utow(ws, FF_COUNT(ws), name)))
		return -1;
	int r = !CreateDirectoryW(w, NULL);
	if (w != ws)
		ffmem_free(w);
	return r;
}

static inline int ffdir_make_all(char *path, ffsize off)
{
	wchar_t ws[256], *w;
	if (NULL == (w = ffsz_alloc_buf_utow(ws, FF_COUNT(ws), path)))
		return -1;

	int first = w[0] | 0x20;
	if (off == 0
		&& first >= 'a' && first <= 'z'
		&& w[1] == ':')
		off = FFS_LEN("c:"); // don't try to make directory "drive:"

	if (w[off] == '/' || w[off] == '\\')
		off++; // don't try to make directory "/"

	int r = 0;
	for (;; off++) {

		int c = w[off];
		if (c == '/' || c == '\\' || c == '\0') {

			w[off] = '\0';
			r = !CreateDirectoryW(w, NULL);
			w[off] = c;

			if (r != 0 && GetLastError() != ERROR_ALREADY_EXISTS)
				goto end;

			if (c == '\0')
				break;
		}
	}

end:
	if (w != ws)
		ffmem_free(w);
	return r;
}

static inline int ffdir_remove(const char *name)
{
	wchar_t ws[256], *w;
	if (NULL == (w = ffsz_alloc_buf_utow(ws, FF_COUNT(ws), name)))
		return -1;
	int r = !RemoveDirectoryW(w);
	if (w != ws)
		ffmem_free(w);
	return r;
}


typedef fffd ffdir;
typedef BY_HANDLE_FILE_INFORMATION fffileinfo;

typedef struct ffdirentry {
	fffileinfo info;
	WIN32_FIND_DATAW find_data;
	char name[256*4];
	ffuint next;
} ffdirentry;

static inline ffdir ffdir_open(char *path, ffsize cap, ffdirentry *ent)
{
	ffsize len = ffsz_len(path);
	if (len == 0 || len + 2 >= cap) {
		SetLastError(ERROR_INVALID_DATA);
		return NULL;
	}

	wchar_t ws[256], *w;

	// "/dir" -> "/dir\\*"
	int last = path[len - 1];
	char *end = path + len;
	if (!(last == '/' || last == '\\'))
		*end++ = '\\';
	*end++ = '*';
	*end = '\0';
	w = ffsz_alloc_buf_utow(ws, FF_COUNT(ws), path);
	path[len] = '\0';

	if (w == NULL)
		return NULL;

	ffdir dir = FindFirstFileW(w, &ent->find_data);
	if (dir == INVALID_HANDLE_VALUE) {
		dir = NULL;
		goto end;
	}
	ent->next = 0;

end:
	if (w != ws)
		ffmem_free(w);
	return dir;
}

static inline void ffdir_close(ffdir dir)
{
	FindClose(dir);
}

static inline int ffdir_read(ffdir dir, ffdirentry *ent)
{
	if (!ent->next) {
		ent->next = 1;
	} else {
		if (0 == FindNextFileW(dir, &ent->find_data))
			return -1;
	}

	if (0 >= ffsz_wtou(ent->name, sizeof(ent->name), ent->find_data.cFileName)) {
		SetLastError(ERROR_INVALID_DATA);
		return -1;
	}
	return 0;
}

static inline const char* ffdir_entry_name(ffdirentry *ent)
{
	return ent->name;
}

static inline fffileinfo* ffdir_entry_info(ffdirentry *ent)
{
	const WIN32_FIND_DATAW *fd = &ent->find_data;
	fffileinfo *fi = &ent->info;
	fi->dwFileAttributes = fd->dwFileAttributes;
	fi->ftCreationTime = fd->ftCreationTime;
	fi->ftLastAccessTime = fd->ftLastAccessTime;
	fi->ftLastWriteTime = fd->ftLastWriteTime;
	fi->nFileSizeHigh = fd->nFileSizeHigh;
	fi->nFileSizeLow = fd->nFileSizeLow;
	fi->nFileIndexHigh = fi->nFileIndexLow = 0;
	return &ent->info;
}

#else // UNIX:

#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#define FFERR_DIREXISTS  EEXIST
#define FFERR_NOMOREFILES  0

static inline int ffdir_make(const char *name)
{
	return mkdir(name, 0777);
}

static inline int ffdir_make_all(char *path, ffsize off)
{
	if (path[off] == '/')
		off++; // don't try to make directory "/"

	for (;; off++) {

		int c = path[off];
		if (c == '/' || c == '\0') {

			path[off] = '\0';
			int r = ffdir_make(path);
			path[off] = c;

			if (r != 0 && errno != EEXIST)
				return r;

			if (c == '\0')
				break;
		}
	}

	return 0;
}

static inline int ffdir_remove(const char *name)
{
	return rmdir(name);
}


typedef DIR* ffdir;
#ifndef fffileinfo // may be defined in FFOS/file.h
	#define fffileinfo  struct stat
#endif

typedef struct ffdirentry {
	fffileinfo info;
	ffsize path_len;
	char *path;
	ffsize path_cap;
	struct dirent *de;
} ffdirentry;

static inline ffdir ffdir_open(char *path, ffsize path_cap, ffdirentry *ent)
{
	ent->path_len = ffsz_len(path);
	ent->path = path;
	ent->path_cap = path_cap;
	return opendir(path);
}

static inline void ffdir_close(ffdir d)
{
	closedir(d);
}

static inline int ffdir_read(ffdir dir, ffdirentry *ent)
{
	errno = FFERR_NOMOREFILES;
	if (NULL == (ent->de = readdir(dir))) {
		return -1;
	}
	return 0;
}

static inline const char* ffdir_entry_name(ffdirentry *ent)
{
	return ent->de->d_name;
}

/* "path" + "name" -> "path/name" */
static inline const char* _ffdir_entry_fullpath(ffdirentry *ent, const char *name, ffsize name_len)
{
	if (ent->path_len + name_len + 1 >= ent->path_cap) {
		errno = EOVERFLOW;
		return NULL;
	}
	ent->path[ent->path_len] = '/';
	ffmem_copy(ent->path + ent->path_len + 1, name, name_len);
	ent->path[ent->path_len + name_len + 1] = '\0';
	return ent->path;
}

static inline fffileinfo* ffdir_entry_info(ffdirentry *ent)
{
	const char *name = ffdir_entry_name(ent);
	if (NULL == (name = _ffdir_entry_fullpath(ent, name, ffsz_len(name))))
		return NULL;
	int r = stat(name, &ent->info);
	ent->path[ent->path_len] = '\0';
	return (r == 0) ? &ent->info : NULL;
}

#endif


static inline int ffdir_make_path(char *name, ffsize off)
{
	ffsize n = ffsz_len(name);
	ffssize slash;
#ifdef FF_WIN
	slash = ffs_rfindany(name, n, "/\\", 2);
#else
	slash = ffs_rfindchar(name, n, '/');
#endif
	if (slash <= 0)
		return 0;

	int c = name[slash];
	name[slash] = '\0';
	int r = ffdir_make_all(name, off);
	name[slash] = c;
	return r;
}


/** Create a new directory
Return !=0 on error
  errno=FFERR_DIREXISTS: already exists */
static int ffdir_make(const char *name);

/** Recursively create directories
off: length (in bytes) of the path that already exists
Example: ffdir_make_all("/a/b/c", 2) will create "/a/b" & "/a/b/c" */
static int ffdir_make_all(char *path, ffsize off);

/** Create directory(s) for a file */
static int ffdir_make_path(char *name, ffsize off);

/** Delete the directory */
static int ffdir_remove(const char *name);


/** Open directory reader
path: path to a directory
  Should have at least 256 characters of free space,
   because ffdir_entry_info() must append a file name (UNIX)
   and ffdir_open() must append "\\*" (Windows).
UNIX: doesn't fetch file information automatically
Return NULL on error  */
static ffdir ffdir_open(char *path, ffsize path_cap, ffdirentry *ent);

/** Close directory reader */
static void ffdir_close(ffdir d);

/** Get next entry from directory list (not sorted)
UNIX: not thread-safe if multiple threads use the same 'dir' descriptor
Return !=0 on error
  errno=FFERR_NOMOREFILES: there are no more files */
static int ffdir_read(ffdir dir, ffdirentry *ent);

/** Get entry name */
static const char* ffdir_entry_name(ffdirentry *ent);

/** Get file information
Return NULL on error */
static fffileinfo* ffdir_entry_info(ffdirentry *ent);


#ifndef FFOS_NO_COMPAT
#define ffdir_rm  ffdir_remove
#define ffdir_rmake  ffdir_make_all
#define ffdir_entryinfo  ffdir_entry_info
#ifdef FF_WIN
#define ffdir_entryname(ent)  ((ent)->find_data.cFileName)
#else
#define ffdir_entryname  ffdir_entry_name
#endif
#define ENOMOREFILES  FFERR_NOMOREFILES
#include <FFOS/path.h>
#endif

/** ffos: file mapping
2020, Simon Zolin
*/

/*
ffmmap_create
ffmmap_open
ffmmap_unmap
ffmmap_close
*/

#pragma once

#include <FFOS/base.h>

#ifdef FF_WIN

enum FFMMAP_PAGEACCESS {
	FFMMAP_READ = PAGE_READONLY,
	FFMMAP_READWRITE = PAGE_READWRITE,
};

enum FFMMAP_PROT {
	PROT_READ = FILE_MAP_READ,
	PROT_WRITE = FILE_MAP_WRITE,
	PROT_EXEC = FILE_MAP_EXECUTE,
};

enum FFMMAP_FLAGS {
	MAP_SHARED = 0,
	MAP_ANONYMOUS = 0,
};

static inline fffd ffmmap_create(fffd fd, ffuint64 max_size, enum FFMMAP_PAGEACCESS access)
{
	fffd fmap = CreateFileMapping(fd, NULL, access, (ffuint)(max_size >> 32), (ffuint)max_size, NULL);
	if (fmap == NULL && GetLastError() == ERROR_FILE_INVALID) //if file size is 0
		fmap = (fffd)-2;
	return fmap;
}

static inline void* ffmmap_open(fffd fmap, ffuint64 offset, ffsize size, int prot, int flags)
{
	(void)flags;
	return MapViewOfFile(fmap, prot, (ffuint)(offset >> 32), (ffuint)offset, size);
}

static inline int ffmmap_unmap(void *p, ffsize sz)
{
	(void)sz;
	return 0 == UnmapViewOfFile(p);
}

static inline void ffmmap_close(fffd fmap)
{
	if (fmap != (fffd)-2)
		CloseHandle(fmap);
}

#else // UNIX:

#include <sys/mman.h>

enum FFMMAP_PAGEACCESS {
	FFMMAP_READ = 0,
	FFMMAP_READWRITE = 0,
};

static inline fffd ffmmap_create(fffd fd, ffuint64 max_size, enum FFMMAP_PAGEACCESS access)
{
	(void)max_size; (void)access;
	return fd;
}

static inline void* ffmmap_open(fffd fd, ffuint64 offset, ffsize size, int prot, int flags)
{
	void *h = mmap(NULL, size, prot, flags, fd, offset);
	return (h != MAP_FAILED) ? h : NULL;
}

static inline int ffmmap_unmap(void *p, ffsize sz)
{
	return munmap(p, sz);
}

static inline void ffmmap_close(fffd fmap)
{
	(void)fmap;
}

#endif


/** Create file mapping
fd: must not be 0
  Use FFFILE_NULL to create anonymous mapping
max_size: max size of the mapping
  Use 0 to map the whole file
Return 0 on error */
static fffd ffmmap_create(fffd fd, ffuint64 max_size, enum FFMMAP_PAGEACCESS access);

/** Map files or devices into memory
prot: PROT_...
flags: MAP_...
Return pointer;  unmap with ffmmap_unmap()
  NULL on error */
static void* ffmmap_open(fffd fd, ffuint64 offset, ffsize size, int prot, int flags);

/** Unmap */
static int ffmmap_unmap(void *p, ffsize sz);

/** Close a file mapping */
static void ffmmap_close(fffd fmap);

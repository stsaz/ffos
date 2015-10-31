/**
Copyright (c) 2013 Simon Zolin
*/

enum FFMAP_PAGEACCESS {
	FFMAP_PAGEREAD = 0
	, FFMAP_PAGERW = 0
};

/** Create file mapping.
'fd' must not be 0.
Use 'fd=FF_BADFD' to create anonymous mapping.
Use 'maxSize=0' to map the whole file.
Return 0 on error. */
#define ffmap_create(fd, maxSize, access)  (fd)

/** Map files or devices into memory.
'prot': PROT_*
'flags': MAP_*
Return NULL on error. */
static FFINL void *ffmap_open(fffd fd, uint64 offset, size_t size, int prot, int flags) {
	void *h = mmap(NULL, size, prot, flags, fd, offset);
	return (h != MAP_FAILED) ? h : NULL;
}

/** Unmap. */
#define ffmap_unmap  munmap

/** Close a file mapping. */
#define ffmap_close(h)  ((void)h)

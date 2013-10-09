/**
Copyright (c) 2013 Simon Zolin
*/

enum FFMAP_PAGEACCESS {
	FFMAP_PAGEREAD = PAGE_READONLY
	, FFMAP_PAGERW = PAGE_READWRITE
};

enum FFMAP_PROT {
	PROT_READ = FILE_MAP_READ
	, PROT_WRITE = FILE_MAP_WRITE
	, PROT_EXEC = FILE_MAP_EXECUTE
};

enum FFMAP_FLAGS {
	MAP_SHARED = 0
	, MAP_ANONYMOUS = 0
};

static FFINL fffd ffmap_create(fffd fd, uint64 maxSize, enum FFMAP_PAGEACCESS access) {
	fffd fmap = CreateFileMapping(fd, NULL, access, FF_HI32(maxSize), FF_LO32(maxSize), NULL);
	if (fmap == NULL && GetLastError() == ERROR_FILE_INVALID) //if file size is 0
		fmap = (fffd)-2;
	return fmap;
}

static FFINL void *ffmap_open(fffd hfilemap, uint64 offs, size_t size, int prot, int flags) {
	return MapViewOfFile(hfilemap, prot, FF_HI32(offs), FF_LO32(offs), size);
}

#define ffmap_unmap(p, sz)  (0 == UnmapViewOfFile(p))

static FFINL void ffmap_close(fffd fmap) {
	if (fmap != (fffd)-2)
		CloseHandle(fmap);
}

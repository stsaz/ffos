/**
Copyright (c) 2013 Simon Zolin
*/

/** Heap handle used by ffmem* functions. */
FF_EXTN HANDLE _ffheap;

static FFINL void ffos_init() {
	_ffheap = GetProcessHeap();
}

#define ffmem_alloc(size)  HeapAlloc(_ffheap, 0, size)

#define ffmem_calloc(numElements, szofElement)\
	HeapAlloc(_ffheap, HEAP_ZERO_MEMORY, (numElements) * (szofElement))

static FFINL void *ffmem_realloc(void *ptr, size_t newSize) {
	if (ptr == NULL)
		return ffmem_alloc(newSize);
	return HeapReAlloc(_ffheap, 0, ptr, newSize);
}

#define ffmem_free(ptr)  HeapFree(_ffheap, 0, ptr)

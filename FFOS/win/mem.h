/**
Copyright (c) 2013 Simon Zolin
*/

/** Heap handle used by ffmem* functions. */
FF_EXTN HANDLE _ffheap;

/** Initialize heap memory. */
#define ffmem_init() \
	_ffheap = GetProcessHeap()

#define _ffmem_alloc(size)  HeapAlloc(_ffheap, 0, size)

#define _ffmem_calloc(numElements, szofElement)\
	HeapAlloc(_ffheap, HEAP_ZERO_MEMORY, (numElements) * (szofElement))

static FFINL void *_ffmem_realloc(void *ptr, size_t newSize) {
	if (ptr == NULL)
		return _ffmem_alloc(newSize);
	return HeapReAlloc(_ffheap, 0, ptr, newSize);
}

static inline void _ffmem_free(void *ptr)
{
	HeapFree(_ffheap, 0, ptr);
}


FF_EXTN void* _ffmem_align(size_t size, size_t align);

static FFINL void _ffmem_alignfree(void *ptr)
{
	void *buf = *((void**)ptr - 1);
	_ffmem_free(buf);
}

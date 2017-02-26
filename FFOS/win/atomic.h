/**
Copyright (c) 2013 Simon Zolin
*/


#ifdef FF_64
typedef volatile LONGLONG ffatomic;

static FFINL int ffatom_cmpxchg(ffatomic *a, ssize_t old, ssize_t newval) {
	return old == InterlockedCompareExchange64(a, newval, old);
}
#define ffatom_xchg  InterlockedExchange64

#define ffatom_add  InterlockedExchangeAdd64
#define ffatom_inc  InterlockedIncrement64
#define ffatom_dec  InterlockedDecrement64
#define ffatom_addret(a, add)  (InterlockedExchangeAdd64(a, add) + add)
#define ffatom_incret  InterlockedIncrement64
#define ffatom_decret  InterlockedDecrement64

#else //32-bit:
typedef volatile LONG ffatomic;

static FFINL int ffatom_cmpxchg(ffatomic *a, ssize_t old, ssize_t newval) {
	return old == InterlockedCompareExchange(a, newval, old);
}
#define ffatom_xchg  InterlockedExchange

#define ffatom_add  InterlockedExchangeAdd
#define ffatom_inc  InterlockedIncrement
#define ffatom_dec  InterlockedDecrement
#define ffatom_addret(a, add)  (InterlockedExchangeAdd(a, add) + add)
#define ffatom_incret  InterlockedIncrement
#define ffatom_decret  InterlockedDecrement
#endif

#define ffmem_barrier  MemoryBarrier

#ifdef FF_MSVC
#define ffcpu_pause  YieldProcessor
#else
static FFINL void ffcpu_pause() {
	YieldProcessor();
}
#endif

#define ffcpu_yield  SwitchToThread

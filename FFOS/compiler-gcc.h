/** GCC functions.
Copyright (c) 2017 Simon Zolin
*/


typedef long long int64;
typedef unsigned long long uint64;

#define FF_FUNC __func__

#define FFINL  inline

#ifdef FF_MINGW
	#define FF_EXP  __declspec(dllexport)
	#define FF_IMP  __declspec(dllimport)
#else
	#define FF_EXP  __attribute__((visibility("default")))
	#define FF_IMP
#endif


#define ffbit_ffs32(i)  __builtin_ffs(i)

#ifdef FF_64
#define ffbit_ffs64(i)  __builtin_ffsll(i)
#endif

/** ffos: dynamic library
2020, Simon Zolin
*/

/*
ffdl_open
ffdl_close
ffdl_addr
ffdl_errstr
*/

#pragma once

#include <FFOS/base.h>

#ifdef FF_WIN

#include <FFOS/string.h>
#include <FFOS/error.h>

#define FFDL_EXT  "dll"
typedef HMODULE ffdl;
#define FFDL_NULL  NULL

enum FFDL_OPEN {
	/** Look for dependent libs in:
	 1. the same dir as the lib being loaded
	 2. 'system32' dir
	This changes default search path which is:
	 1. the app dir
	     or the same dir as the lib being loaded (LOAD_WITH_ALTERED_SEARCH_PATH)
	 2. 'system32' & 'windows' dirs
	 3. current dir (or #2 if SafeDllSearchMode=0)
	 4. dirs from %PATH% */
	FFDL_SELFDIR = LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32,
};

/** Check if LoadLibraryExW() supports LOAD_LIBRARY_SEARCH_... flags
Return 1: supports;
 -1: doesn't support */
static int _ffdl_init_flags()
{
	int r = 1;
	ffdl dl;
	if (NULL == (dl = LoadLibraryExW(L"kernel32.dll", NULL, 0)))
		return -1;
	if (NULL == GetProcAddress(dl, "AddDllDirectory"))
		r = -1;
	FreeLibrary(dl);
	return r;
}

FF_EXTERN signed char _ffdl_open_supports_flags;

static inline ffdl ffdl_open(const char *filename, ffuint flags)
{
	wchar_t ws[256], *w;
	if (NULL == (w = ffsz_alloc_buf_utow(ws, FF_COUNT(ws), filename)))
		return FFDL_NULL;

	if (flags != 0) {
		int f = _ffdl_open_supports_flags;
		if (f == 0) {
			f = _ffdl_init_flags();
			_ffdl_open_supports_flags = f;
		}
		if (f < 0)
			flags = LOAD_WITH_ALTERED_SEARCH_PATH;
	}

	ffdl dl = LoadLibraryExW(w, NULL, flags);

	if (w != ws)
		ffmem_free(w);
	return dl;
}

static inline void ffdl_close(ffdl dl)
{
	FreeLibrary(dl);
}

static inline void* ffdl_addr(ffdl dl, const char *proc_name)
{
	return (void*)GetProcAddress(dl, proc_name);
}

static inline const char* ffdl_errstr()
{
	return fferr_strptr(GetLastError());
}

#else // UNIX:

#include <dlfcn.h>

#ifdef FF_APPLE
	#define FFDL_EXT  "dylib"
#else
	#define FFDL_EXT  "so"
#endif

typedef void* ffdl;
#define FFDL_NULL  NULL

enum FFDL_OPEN {
	FFDL_SELFDIR = 0,
};

static inline ffdl ffdl_open(const char *filename, ffuint flags)
{
	return dlopen(filename, flags | RTLD_LAZY);
}

static inline void ffdl_close(ffdl dl)
{
	dlclose(dl);
}

static inline void* ffdl_addr(ffdl dl, const char *proc_name)
{
	return dlsym(dl, proc_name);
}

static inline const char* ffdl_errstr()
{
	return dlerror();
}

#endif

/** Open library
flags: enum FFDL_OPEN
  FFDL_SELFDIR:
    Windows: Load dependencies from the library's directory first
      Filename parameter must be absolute and must have '\\' (not '/').
    UNIX: The same behaviour is achieved by using linker flag '-Wl,-rpath,$$ORIGIN'
Return NULL on error */
static ffdl ffdl_open(const char *filename, ffuint flags);

/** Close the library */
static void ffdl_close(ffdl dl);

/** Get symbol address from the library
Return NULL on error */
static void* ffdl_addr(ffdl dl, const char *proc_name);

/** Get last error message from ffdl_open() or ffdl_addr() */
static const char* ffdl_errstr();

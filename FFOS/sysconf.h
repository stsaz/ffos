/** ffos: system config
2020, Simon Zolin
*/

/*
ffsysconf_init
ffsysconf_get
*/

#pragma once

#include <FFOS/base.h>

#ifdef FF_WIN

enum FFSYSCONF_ID {
	FFSYSCONF_PAGESIZE = 1,
	FFSYSCONF_NPROCESSORS_ONLN,
};

typedef SYSTEM_INFO ffsysconf;

static inline void ffsysconf_init(ffsysconf *sc)
{
	GetNativeSystemInfo(sc);
}

static inline int ffsysconf_get(ffsysconf *sc, int id)
{
	switch (id) {
	case FFSYSCONF_PAGESIZE:
		return sc->dwAllocationGranularity;
	case FFSYSCONF_NPROCESSORS_ONLN:
		return sc->dwNumberOfProcessors;
	}
	return 0;
}

#else // UNIX:

typedef ffuint ffsysconf;

static inline void ffsysconf_init(ffsysconf *sc)
{
	(void)sc;
}

enum FFSYSCONF_ID {
	FFSYSCONF_PAGESIZE = _SC_PAGESIZE,
	FFSYSCONF_NPROCESSORS_ONLN = _SC_NPROCESSORS_ONLN,
};

static inline int ffsysconf_get(ffsysconf *sc, int id)
{
	(void)sc;
	return sysconf(id);
}

#endif

/** Init sysconf structure */
static void ffsysconf_init(ffsysconf *sc);

/** Get value from system config
id: enum FFSYSCONF_ID */
static int ffsysconf_get(ffsysconf *sc, int id);

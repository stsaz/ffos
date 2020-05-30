/**
Copyright (c) 2013 Simon Zolin
*/

#ifdef FF_LINUX
#include <FFOS/linux/systimer.h>
#endif
#include <FFOS/mem.h>
#include <FFOS/error.h>
#include <ffbase/string.h>
#include <ffbase/stringz.h>
#include <signal.h>
#include <dlfcn.h>


typedef int ffps;
enum { FFPS_INV = -1 };

/** Process fork.
Return child PID.  Return 0 in the child process.
Return -1 on error. */
#define ffps_fork  fork

/** Get process ID by process handle. */
#define ffps_id(h)  ((uint)(h))

/** Kill the process. */
#define ffps_kill(h)  kill(h, SIGKILL)

/** Send signal to a process. */
#define ffps_sig(h, sig)  kill(h, sig)

/** Close process handle. */
static inline void ffps_close(ffps h){}

/** Get the current process handle. */
#define ffps_curhdl  getpid

/** Get ID of the current process. */
#define ffps_curid  getpid

/** Exit the current process. */
#define ffps_exit  _exit


#ifdef FF_APPLE
#define FFDL_EXT  "dylib"
#else
#define FFDL_EXT  "so"
#endif

typedef void * ffdl;
typedef void * ffdl_proc;

enum FFDL_OPEN {
	/** Windows: load dependencies from the library's directory first.
	Filename parameter must be absolute and must not contain '/'. */
	FFDL_SELFDIR = 0,
};

/** Open library.
@flags: enum FFDL_OPEN
Return NULL on error. */
#define ffdl_open(filename, flags)  dlopen(filename, (flags) | RTLD_LAZY)

/** Get the function address from the library.
Return NULL on error. */
#define ffdl_addr(dl, name)  dlsym(dl, name)

/** Get last error message. */
#define ffdl_errstr()  dlerror()

/** Close the library. */
#define ffdl_close(dl)  dlclose(dl)


typedef void * ffsysconf;

/** Init sysconf structure. */
#define ffsc_init(sc) ((void)sc)

/** Get value from system config. */
#define ffsc_get(sc, name)  sysconf(name)


static inline ffstr _ffsvar_next(ffstr *s)
{
	ffstr out;
	out.ptr = s->ptr;

	if (s->ptr[0] != '$') {
		ffssize pos = ffstr_findchar(s, '$');
		if (pos < 0)
			pos = s->len;
		out.len = pos;
		ffstr_shift(s, pos);
		return out;
	}

	ffsize i;
	for (i = 1;  i < s->len;  i++) {
		int ch = s->ptr[i];
		// "a-z A-Z 0-9 _"
		if (((ch | 0x20) >= 'a' && (ch | 0x20) <= 'z')
			|| (ch >= '0' && ch <= '9')
			|| ch == '_')
			continue;
		break;
	}
	out.len = i;
	ffstr_shift(s, i);
	return out;
}

/** Get environment variable's value
Retun NULL on error */
static inline const char* ffenv_val(const char *key, ffsize keylen)
{
	for (ffuint i = 0;  environ[i] != NULL;  i++) {
		const char *e = environ[i];

		if (ffsz_match(e, key, keylen)
			&& e[keylen] == '=') {
			return e + keylen + 1;
		}
	}
	return NULL;
}

static inline char* ffenv_expand(void *unused, char *dst, size_t cap, const char *src)
{
	ffstr in, s, out;
	ffstr_setz(&in, src);
	ffstr_set(&out, dst, 0);
	if (dst == NULL)
		cap = 0;

	while (in.len != 0) {
		s = _ffsvar_next(&in);

		if (s.ptr[0] == '$') {

			ffstr name = s;
			ffstr_shift(&name, 1);
			if (name.len == 0)
				continue;

			const char *val = ffenv_val(name.ptr, name.len);
			s.len = 0;
			if (val != NULL)
				ffstr_setz(&s, val);
		}

		if (dst == NULL
			&& NULL == ffstr_growtwice(&out, &cap, s.len))
			return NULL;

		ffstr_add2(&out, cap, &s);
	}

	if (dst == NULL) {
		if (0 == ffstr_growaddchar(&out, &cap, '\0'))
			return NULL;
	} else {
		ffstr_addchar(&out, cap, '\0');
	}

	return out.ptr;
}


enum FFLANG_F {
	FFLANG_FLANG = 1, // enum FFLANG
};

enum FFLANG {
	FFLANG_NONE,
	FFLANG_GER,
	FFLANG_ENG,
	FFLANG_ESP,
	FFLANG_FRA,
	FFLANG_RUS,
};

/** Get user locale information.
flags: enum FFLANG_F
Return value or -1 on error. */
static inline int fflang_info(uint flags)
{
	if (flags != FFLANG_FLANG)
		return -1;

	const char *val;
	if (NULL == (val = ffenv_val("LANG", 4)))
		return FFLANG_NONE;
	// "lang_COUNTRY.UTF-8"

	if (ffsz_len(val) < FFS_LEN("xx_")
		|| val[2] != '_')
		return FFLANG_NONE;

	static const char langstr_sorted[][2] = {
		"de", // FFLANG_GER
		"en", // FFLANG_ENG
		"es", // FFLANG_ESP
		"fr", // FFLANG_FRA
		"ru", // FFLANG_RUS
	};
	int i = ffarrint16_binfind((ffushort*)langstr_sorted, FF_COUNT(langstr_sorted), *(ffushort*)val);
	if (i < 0)
		return FFLANG_NONE;
	return i + 1;
}

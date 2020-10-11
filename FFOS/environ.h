/** ffos: environment
2020, Simon Zolin
*/

/*
ffenv_init
ffenv_expand
ffenv_locale
*/

#pragma once

#include <FFOS/base.h>

#ifdef FF_WIN

#include <FFOS/string.h>

static inline int ffenv_update()
{
	DWORD_PTR r;
	return !SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"Environment", SMTO_ABORTIFHUNG, 5000, &r);
}

static inline int ffenv_init(void *unused, char **env)
{
	(void)unused; (void)env;
	return 0;
}

static inline char* ffenv_expand(void *unused, char *dst, ffsize dst_cap, const char *src)
{
	(void)unused;
	wchar_t ws[256], *wsrc, *wdst = NULL;
	ffsize wlen;

	if (NULL == (wsrc = ffsz_alloc_buf_utow(ws, FF_COUNT(ws), src)))
		return NULL;

	wlen = ExpandEnvironmentStringsW(wsrc, NULL, 0);
	if (NULL == (wdst = (wchar_t*)ffmem_alloc(wlen * sizeof(wchar_t)))) {
		dst = NULL;
		goto end;
	}
	ExpandEnvironmentStringsW(wsrc, wdst, wlen);

	if (dst == NULL) {
		dst_cap = ffs_wtou(NULL, 0, wdst, wlen);
		if (NULL == (dst = (char*)ffmem_alloc(dst_cap)))
			goto end;
	}
	ffs_wtou(dst, dst_cap, wdst, wlen);

end:
	if (wsrc != ws)
		ffmem_free(wsrc);
	ffmem_free(wdst);
	return dst;
}

enum FFENV_F {
	FFENV_LANGUAGE = LOCALE_ILANGUAGE, // enum FFLANG
};

enum FFLANG {
	FFLANG_NONE,
	FFLANG_ENG = LANG_ENGLISH,
	FFLANG_ESP = LANG_SPANISH,
	FFLANG_FRA = LANG_FRENCH,
	FFLANG_GER = LANG_GERMAN,
	FFLANG_IND = LANG_INDONESIAN,
	FFLANG_RUS = LANG_RUSSIAN,
};

static inline int ffenv_locale(ffuint flags)
{
	wchar_t val[4];
	GetLocaleInfoW(LOCALE_USER_DEFAULT, flags | LOCALE_RETURN_NUMBER, val, FF_COUNT(val));
	return *(ffushort*)val & 0xff;
}

#else // UNIX:

#include <FFOS/string.h>
#include <ffbase/stringz.h>

/** Get the next chunk of text or $-variable name */
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

FF_EXTERN char **_ff_environ;

static inline int ffenv_init(void *unused, char **env)
{
	(void)unused;
	_ff_environ = env;
	return 0;
}

/** Get environment variable's value
Retun NULL on error */
static inline const char* _ffenv_val(const char *key, ffsize keylen)
{
	for (ffuint i = 0;  _ff_environ[i] != NULL;  i++) {
		const char *e = _ff_environ[i];

		if (ffsz_match(e, key, keylen)
			&& e[keylen] == '=') {
			return e + keylen + 1;
		}
	}
	return NULL;
}

static inline char* ffenv_expand(void *unused, char *dst, ffsize dst_cap, const char *src)
{
	(void)unused;
	ffstr in, s, out;
	ffstr_setz(&in, src);
	ffstr_set(&out, dst, 0);
	if (dst == NULL)
		dst_cap = 0;

	while (in.len != 0) {
		s = _ffsvar_next(&in);

		if (s.ptr[0] == '$') {

			ffstr name = s;
			ffstr_shift(&name, 1);
			if (name.len == 0)
				continue;

			const char *val = _ffenv_val(name.ptr, name.len);
			s.len = 0;
			if (val != NULL)
				ffstr_setz(&s, val);
		}

		if (dst == NULL
			&& NULL == ffstr_growtwice(&out, &dst_cap, s.len))
			return NULL;

		ffstr_add2(&out, dst_cap, &s);
	}

	if (dst == NULL) {
		if (0 == ffstr_growaddchar(&out, &dst_cap, '\0'))
			return NULL;
	} else {
		ffstr_addchar(&out, dst_cap, '\0');
	}

	return out.ptr;
}


enum FFENV_F {
	FFENV_LANGUAGE = 1, // enum FFLANG
};

enum FFLANG {
	FFLANG_NONE,
	FFLANG_GER,
	FFLANG_ENG,
	FFLANG_ESP,
	FFLANG_FRA,
	FFLANG_IND,
	FFLANG_RUS,
};

static int _fflang_cmp(const void *a, const void *b)
{
	return ffmem_cmp(a, b, 2);
}

/** Get user locale information
flags: enum FFENV_F
Return a value
  -1 on error */
static inline int ffenv_locale(ffuint flags)
{
	if (flags != FFENV_LANGUAGE)
		return -1;

	const char *val;
	if (NULL == (val = _ffenv_val("LANG", 4)))
		return FFLANG_NONE;
	// "lang_COUNTRY.UTF-8"

	if (ffsz_len(val) < FFS_LEN("xx_")
		|| val[2] != '_')
		return FFLANG_NONE;

	static const char langstr_sorted[][2] = {
		{'d','e'}, // FFLANG_GER
		{'e','n'}, // FFLANG_ENG
		{'e','s'}, // FFLANG_ESP
		{'f','r'}, // FFLANG_FRA
		{'i','d'}, // FFLANG_IND
		{'r','u'}, // FFLANG_RUS
	};
	int i = ffarr_binfind(langstr_sorted, FF_COUNT(langstr_sorted), val, 2, _fflang_cmp);
	if (i < 0)
		return FFLANG_NONE;
	return i + 1;
}

#endif


/** Initalize 'environ' global variable */
static int ffenv_init(void *unused, char **env);

/** Expand environment variables in a string
src:
  UNIX: "text $VAR text"
  Windows: "text %VAR% text"
dst: receives expanded string, NULL-terminated
  NULL: allocate new buffer
Return 'dst' or a new allocated buffer
  NULL on error */
static char* ffenv_expand(void *unused, char *dst, ffsize dst_cap, const char *src);

/** Get user locale information
flags: enum FFENV_F */
static int ffenv_locale(ffuint flags);

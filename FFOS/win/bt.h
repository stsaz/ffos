/**
Copyright (c) 2020 Simon Zolin
*/


typedef struct ffthd_bt {
	void *frames[64];
	uint n;
	int i_mod;
	void *base;
	ffsyschar wname[1024];
} ffthd_bt;

static inline int ffthd_backtrace(ffthd_bt *bt)
{
	uint cnt = FFCNT(bt->frames);
#if FF_WIN <= 0x0501
	cnt = ffmin(62, FFCNT(bt->frames));
#endif
	bt->n = CaptureStackBackTrace(0, cnt, bt->frames, NULL);
	bt->i_mod = -1;
	return bt->n;
}

static inline void* ffthd_backtrace_modbase(ffthd_bt *bt, uint i)
{
	if (i >= bt->n)
		return NULL;

	if (i != (uint)bt->i_mod) {
		HMODULE mod;
		uint f = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
			| GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
		if (!GetModuleHandleEx(f, bt->frames[i], &mod))
			return NULL;
		bt->base = mod;
		bt->i_mod = i;
	}

	return bt->base;
}

static inline const ffsyschar* ffthd_backtrace_modname(ffthd_bt *bt, uint i)
{
	if (i >= bt->n || bt->frames[i] == NULL)
		return NULL;

	if (i != (uint)bt->i_mod) {
		HMODULE mod;
		uint f = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
			| GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
		if (!GetModuleHandleEx(f, bt->frames[i], &mod))
			return NULL;
		bt->base = mod;
		bt->i_mod = i;
	}

	bt->wname[0] = '\0';
	(void) GetModuleFileName((HMODULE)bt->base, bt->wname, FFCNT(bt->wname));
	return bt->wname;
}

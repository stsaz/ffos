/** ffos: File system monitoring (Linux, Windows) (incompatible)
2022, Simon Zolin
*/

/*
fffilemon_open fffilemon_close
fffilemon_add | fffilemon_add_dir fffilemon_add_file
fffilemon_rm
fffilemon_read_async | _fffilemon_watch_read_async
fffilemon_next
*/

#include <FFOS/error.h>
#include <FFOS/kqtask.h>
#include <ffbase/string.h>

#ifdef FF_WIN

#include <FFOS/file.h>
#include <FFOS/path.h>
#include <ffbase/vector.h>

typedef struct fffilemon_s* fffilemon;
struct fffilemon_s {
	ffvec wl; // struct _fffilemon_watch*[]
};

struct _fffilemon_watch {
	HANDLE d;
	ffuint events;
	ffuint dir :1;
	char namez[0];
};

#define FFFILEMON_NULL  NULL
#define FFFILEMON_EINPROGRESS  ERROR_IO_PENDING

#define FFFILEMON_EV_CHANGE \
	(FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME /*rename, delete, create*/ \
		| FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE \
		| FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_CREATION | FILE_NOTIFY_CHANGE_SECURITY)

static inline fffilemon fffilemon_open(ffuint flags)
{
	(void)flags;
	return ffmem_new(struct fffilemon_s);
}

static inline void fffilemon_close(fffilemon fm)
{
	if (fm == FFFILEMON_NULL) return;

	struct _fffilemon_watch **pw;
	FFSLICE_WALK(&fm->wl, pw) {
		CloseHandle((*pw)->d);
		ffmem_free(*pw);
	}
	ffvec_free(&fm->wl);
	ffmem_free(fm);
}

static inline int fffilemon_add_dir(fffilemon fm, const char *path, ffuint events)
{
	int rc = -1;
	fffd d = INVALID_HANDLE_VALUE;

	if (INVALID_HANDLE_VALUE == (d = fffile_open(path, FFFILE_READONLY | FILE_FLAG_OVERLAPPED)))
		goto end;

	struct _fffilemon_watch *w, **pw;
	if (NULL == (w = (struct _fffilemon_watch*)ffmem_alloc(sizeof(struct _fffilemon_watch) + 1)))
		goto end;

	if (NULL == (pw = ffvec_pushT(&fm->wl, struct _fffilemon_watch*)))
		goto end;

	w->dir = 1;
	w->d = d;
	d = INVALID_HANDLE_VALUE;
	w->events = events;
	*pw = w;
	rc = fm->wl.len - 1;

end:
	if (d != INVALID_HANDLE_VALUE) {
		CloseHandle(d);
		ffmem_free(w);
	}
	return rc;
}

static inline int fffilemon_add_file(fffilemon fm, const char *path, ffuint events)
{
	int rc = -1;
	ffstr spath = FFSTR_INITZ(path), dir, name;
	ffpath_splitpath_str(spath, &dir, &name);
	char *dirz = NULL;
	fffd d = INVALID_HANDLE_VALUE;

	if (NULL == (dirz = ffsz_dupstr(&dir)))
		goto end;

	if (INVALID_HANDLE_VALUE == (d = fffile_open(dirz, FFFILE_READONLY | FILE_FLAG_OVERLAPPED)))
		goto end;

	struct _fffilemon_watch *w, **pw;
	if (NULL == (w = (struct _fffilemon_watch*)ffmem_alloc(sizeof(struct _fffilemon_watch) + name.len + 1)))
		goto end;

	if (NULL == (pw = ffvec_pushT(&fm->wl, struct _fffilemon_watch*)))
		goto end;

	ffsz_copyn(w->namez, -1, name.ptr, name.len);
	w->d = d;
	d = INVALID_HANDLE_VALUE;
	w->events = events;
	*pw = w;
	rc = fm->wl.len - 1;

end:
	if (d != INVALID_HANDLE_VALUE) {
		CloseHandle(d);
		ffmem_free(w);
	}
	ffmem_free(dirz);
	return rc;
}

static inline int fffilemon_rm(fffilemon fm, int wd)
{
	(void)fm; (void)wd;
	SetLastError(ERROR_NOT_SUPPORTED);
	return -1;
}

static inline int _fffilemon_watch_read_async(struct _fffilemon_watch *w, char *buf, ffsize cap, ffkq_task *task)
{
	if (task->active) {
		DWORD read;
		if (!GetOverlappedResult(NULL, &task->overlapped, &read, 0)) {
			if (GetLastError() == ERROR_IO_INCOMPLETE)
				SetLastError(ERROR_IO_PENDING);
			else
				task->active = 0;
			return -1;
		}

		task->active = 0;
		return cap;
	}

	int r = ReadDirectoryChangesW(w->d, buf, ffmin(cap, 0xffffffff), /*bWatchSubtree*/ 0
		, w->events, NULL, &task->overlapped, NULL);
	if (r == 1)
		SetLastError(ERROR_IO_PENDING);
	else if (GetLastError() != ERROR_IO_PENDING)
		return -1;

	task->active = 1;
	return -1;
}

static ffuint _fffilemon_next1(ffstr *input, ffstr *name, ffsize *cap)
{
	if (input->len == 0)
		return -1;
	if (sizeof(FILE_NOTIFY_INFORMATION) > input->len)
		return -2;
	const FILE_NOTIFY_INFORMATION *i = (FILE_NOTIFY_INFORMATION*)input->ptr;
	if (sizeof(FILE_NOTIFY_INFORMATION) + i->FileNameLength > input->len)
		return -2;

	name->len = 0;
	if (i->FileNameLength != 0) {
		int r = ffs_wtou(NULL, 0, i->FileName, i->FileNameLength / sizeof(wchar_t));
		if (r <= 0)
			return -2;
		if (NULL == ffstr_grow(name, cap, r))
			return -2;
		name->len = ffs_wtou(name->ptr, *cap, i->FileName, i->FileNameLength / sizeof(wchar_t));
	}

	ffstr_shift(input, i->NextEntryOffset);
	if (i->NextEntryOffset == 0)
		ffstr_shift(input, input->len);

	return i->Action;
}

static inline int fffilemon_next(fffilemon fm, ffstr *input, ffstr *name, ffsize *cap, ffuint *events)
{
	for (;;) {
		int r = _fffilemon_next1(input, name, cap);
		if (r < 0)
			return r;

		struct _fffilemon_watch *w, **pw;
		FFSLICE_WALK(&fm->wl, pw) {
			w = *pw;
			if (w->dir || ffstr_eqz(name, w->namez)) {
				*events = r;
				return pw - (struct _fffilemon_watch**)fm->wl.ptr;
			}
		}
	}
}

#elif defined FF_LINUX

#include <sys/inotify.h>
typedef int fffilemon;
#define FFFILEMON_NULL  (-1)
#define FFFILEMON_EINPROGRESS  EINPROGRESS

#define FFFILEMON_EV_CHANGE \
	(IN_CREATE | IN_MODIFY | IN_ATTRIB | IN_CLOSE_WRITE \
		| IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO \
		| IN_DELETE_SELF | IN_MOVE_SELF)

static inline fffilemon fffilemon_open(ffuint flags)
{
	return inotify_init1(flags);
}

static inline void fffilemon_close(fffilemon fm)
{
	if (fm == FFFILEMON_NULL) return;

	close(fm);
}

static inline int fffilemon_add(fffilemon fm, const char *path, ffuint events)
{
	return inotify_add_watch(fm, path, events);
}

static inline int fffilemon_rm(fffilemon fm, int wd)
{
	return inotify_rm_watch(fm, wd);
}

static inline int fffilemon_read_async(fffilemon fm, char *buf, ffsize cap, ffkq_task *task)
{
	(void)task;
	ffssize r = read(fm, buf, cap);
	if (r < 0 && errno == EAGAIN)
		errno = EINPROGRESS;
	return r;
}

static inline int fffilemon_next(fffilemon fm, ffstr *input, ffstr *name, ffsize *cap, ffuint *events)
{
	(void)fm; (void)cap;
	if (sizeof(struct inotify_event) > input->len)
		return -1;
	struct inotify_event *ie = (struct inotify_event*)input->ptr;
	ffsize n = sizeof(struct inotify_event) + ie->len;
	if (n > input->len)
		return -1;
	ffstr_shift(input, n);

	ffstr_null(name);
	if (ie->len > 0) {
		ffstr_set(name, ie->name, ie->len - 1);
		ffstr_rskipchar(name, '\0');
	}

	*events = ie->mask;
	return ie->wd;
}

#endif

/**
flags: (Linux) IN_NONBLOCK
Return FFFILEMON_NULL on error */
static fffilemon fffilemon_open(ffuint flags);

/** Close fffilemon descriptor */
static void fffilemon_close(fffilemon fm);

/** Add path to watch list.
Linux:
  events: mask of FFFILEMON_EV_... or IN_...
Windows:
  Not implemented, use fffilemon_add_dir() and fffilemon_add_file().
  events: mask of FFFILEMON_EV_... or FILE_NOTIFY_CHANGE...
Return watch descriptor;
  -1 on error */
// static int fffilemon_add(fffilemon fm, const char *path, ffuint events);

/** Remove watch item (Linux) */
static int fffilemon_rm(fffilemon fm, int wd);

/** Read data containing the signalled events.
Windows:
  Not implemented, use _fffilemon_watch_read_async() for each watch directory.
  1. Call _fffilemon_watch_read_async() to begin watching directory
  2. Wait for KQ signal
  3. Call _fffilemon_watch_read_async() to get the results
Return N of bytes read;
  -1 with errno=FFFILEMON_EINPROGRESS if can't complete immediately */
// static int fffilemon_read_async(fffilemon fm, char *buf, ffsize cap, ffkq_task *task);

/** Get next event.
Linux:
  name: file name
  events: signalled events mask (IN_...)
Windows:
  name: file name buffer;  user must ffstr_free() it.
  cap: `name` capacity
  events: signalled event (FILE_ACTION_...)
Return signalled watch descriptor;
  -1 if no more events;
  -2 on error (Windows) */
static int fffilemon_next(fffilemon fm, ffstr *input, ffstr *name, ffsize *cap, ffuint *events);

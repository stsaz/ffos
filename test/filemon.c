/** ffos: filemon.h tester
2022, Simon Zolin
*/

#include <FFOS/filemon.h>
#include <FFOS/file.h>
#include <FFOS/test.h>

#ifdef FF_WIN

#include <FFOS/queue.h>

void test_filemon()
{
	const char *watchpath = ".";
	const char *path = "./ffostest-filemon";
	fffile_remove(path);

	fffilemon fm;
	ffuint events = FFFILEMON_EV_CHANGE;
	x_sys(FFFILEMON_NULL != (fm = fffilemon_open(0)));

	int wd;
	x_sys(-1 != (wd = fffilemon_add_dir(fm, watchpath, events)));
	// x_sys(-1 != (wd = fffilemon_add_file(fm, path, events)));
	struct _fffilemon_watch **pw = (struct _fffilemon_watch**)fm->wl.ptr;
	struct _fffilemon_watch *w = *pw;

	ffkq kq = ffkq_create();
	x_sys(0 == ffkq_attach(kq, w->d, &fm, 0));

	char buf[4096];
	ffkq_task task = {};
	int r = _fffilemon_watch_read_async(w, buf, sizeof(buf), &task);
	x_sys(r < 0);
	x_sys(fferr_last() == ERROR_IO_PENDING);

	fffd f;
	x_sys(FFFILE_NULL != (f = fffile_open(path, FFFILE_CREATE | FFFILE_WRITEONLY)));
	fffile_close(f);

	// char *path2 = ffsz_allocfmt("%s.new", path);
	// fffile_rename(path, path2);
	// ffmem_free(path2);

	ffkq_event ev;
	ffkq_time tm;
	ffkq_time_set(&tm, 1000);
	x_sys(1 == ffkq_wait(kq, &ev, 1, tm));
	x(ffkq_event_data(&ev) == &fm);

	r = _fffilemon_watch_read_async(w, buf, sizeof(buf), &task);
	x_sys(r >= 0);
	ffstr s = FFSTR_INITN(buf, r);

	ffstr name = {};
	ffsize cap = 0;
	for (;;) {
		ffuint ev = 0;
		int wd = fffilemon_next(fm, &s, &name, &cap, &ev);
		if (wd == -1)
			break;
		x(wd >= 0);
		fflog("'%S' wd:%u event:%8xu  added:%u  removed:%u  modified:%u  renamed_old_name:%u  renamed_new_name:%u"
			, &name, wd
			, ev
			, (ev == FILE_ACTION_ADDED)
			, (ev == FILE_ACTION_REMOVED)
			, (ev == FILE_ACTION_MODIFIED)
			, (ev == FILE_ACTION_RENAMED_OLD_NAME)
			, (ev == FILE_ACTION_RENAMED_NEW_NAME)
			);
	}

	fffilemon_close(fm);
	ffkq_close(kq);
	ffstr_free(&name);
	fffile_remove(path);
}

#else

void test_filemon()
{
	const char *watchpath = "/tmp";
	const char *path = "/tmp/ffostest-filemon";
	fffile_remove(path);
	fffd f;
	x_sys(FFFILE_NULL != (f = fffile_open(path, FFFILE_CREATE | FFFILE_WRITEONLY)));

	fffilemon fm;
	x_sys(FFFILEMON_NULL != (fm = fffilemon_open(0)));

	ffuint events = FFFILEMON_EV_CHANGE;
	int wd;
	x_sys(-1 != (wd = fffilemon_add(fm, watchpath, events)));

	// fffd f;
	// x_sys(FFFILE_NULL != (f = fffile_open(path, FFFILE_CREATE | FFFILE_WRITEONLY)));

	// char *path2 = ffsz_allocfmt("%s.new", path);
	// fffile_rename(path, path2);
	// ffmem_free(path2);

	// fffile_remove(path);
	fffile_close(f);

	ffkq_task tsk = {};
	char buf[4096];
	int r = fffilemon_read_async(fm, buf, sizeof(buf), &tsk);
	x_sys(r >= 0);
	ffstr s = FFSTR_INITN(buf, r);

	for (;;) {
		ffstr name;
		ffuint ev;
		int wd = fffilemon_next(fm, &s, &name, NULL, &ev);
		if (wd < 0)
			break;
		fflog("'%S' wd:%u  events:%8xu  wclosed:%u  deleted:%u  deleted-self:%u  moved:%u  ignored:%u"
			, &name, wd
			, ev
			, !!(ev & IN_CLOSE_WRITE)
			, !!(ev & IN_DELETE)
			, !!(ev & IN_DELETE_SELF)
			, !!(ev & IN_MOVE_SELF)
			, !!(ev & IN_IGNORED)
			);
	}

	x_sys(0 == fffilemon_rm(fm, wd));
	fffilemon_close(fm);
	fffile_remove(path);
}

#endif

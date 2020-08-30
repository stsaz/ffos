/**
Copyright (c) 2013 Simon Zolin
*/

#include "all.h"
#include <FFOS/mem.h>
#include <FFOS/string.h>
#include <FFOS/dir.h>
#include <FFOS/file.h>
#include <FFOS/error.h>
#include <FFOS/test.h>
#include <ffbase/stringz.h>

#define x FFTEST_BOOL

static int test_path(const char *dirname)
{
	FFTEST_FUNC;

	x(ffpath_abs(FFSTR("/absolute/path")));
	x(!ffpath_abs(FFSTR("./relative/path")));
#ifdef FF_WIN
	x(ffpath_abs(FFSTR("\\absolute\\path")));
	x(ffpath_abs(FFSTR("c:\\absolute\\path")));
#endif

#ifdef FF_WIN
	{
		ffsyschar wname[FF_MAXPATH];
		if (0 == ff_utow(wname, FFCNT(wname), dirname, -1, 0))
			return -1;
		x(ffpath_islong(wname));
	}
#endif

	return 0;
}

static int test_dirwalk(char *path, size_t pathcap)
{
	ffdir d;
	ffdirentry ent;
	int nfiles = 0;

	FFTEST_FUNC;

	d = ffdir_open(path, pathcap, &ent);
	x(d != NULL);

	for (;;) {
		const char *name;
		const fffileinfo *fi;

		if (0 != ffdir_read(d, &ent)) {
			x(fferr_last() == FFERR_NOMOREFILES);
			break;
		}

		name = ffdir_entryname(&ent);
		x(name != NULL);

		fi = ffdir_entryinfo(&ent);
		x(fi != NULL);

		if (!ffsz_cmp(name, ".")) {
			x(fffile_isdir(fffile_infoattr(fi)));
			nfiles++;
		}
		else if (!ffsz_cmp(name, "tmpfile")) {
			x(!fffile_isdir(fffile_infoattr(fi)));
			x(1 == fffile_infosize(fi));
			nfiles++;
		}
	}
	x(nfiles == 2);

	ffdir_close(d);
	return 0;
}

int test_dir()
{
	fffd f;
	char path[FF_MAXPATH];
	char fn[FF_MAXPATH];
	size_t pathcap = FFCNT(path);

	FFTEST_FUNC;

	strcpy(path, TMP_PATH);
	strcat(path, "/tmpdir");

	strcpy(fn, path);
	strcat(fn, "/tmpfile");

	x(0 == ffdir_make(path));

	f = fffile_open(fn, FFO_CREATE | FFO_TRUNC | FFO_WRONLY);
	x(f != FFFILE_NULL);
	x(1 == fffile_write(f, FFSTR("1")));
	x(0 == fffile_close(f));

	test_dirwalk(path, pathcap);
	test_path(path);

	x(0 == fffile_rm(fn));
	x(0 == ffdir_rm(path));


	strcpy(path, TMP_PATH);
	strcat(path, "/tmpdir/tmpdir2");
	x(0 == ffdir_rmake(path, strlen(TMP_PATH)));
	x(0 == ffdir_rm(path));
	path[strlen(path) - FFSLEN("/tmpdir2")] = '\0';
	x(0 == ffdir_rm(path));

	return 0;
}

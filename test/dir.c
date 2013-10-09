/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/str.h>
#include <FFOS/dir.h>
#include <FFOS/file.h>
#include <FFOS/error.h>
#include <FFOS/test.h>

#define x FFTEST_BOOL

static int test_path(const ffsyschar *dirname)
{
	FFTEST_FUNC;

	x(ffpath_abs(FFSTR("/absolute/path")));
	x(!ffpath_abs(FFSTR("./relative/path")));
#ifdef FF_WIN
	x(ffpath_abs(FFSTR("\\absolute\\path")));
	x(ffpath_abs(FFSTR("c:\\absolute\\path")));
#endif

#ifdef FF_WIN
	x(ffpath_islong(dirname, ffq_len(dirname)));
#endif

	return 0;
}

static int test_dirwalk(ffsyschar *path, size_t pathcap)
{
	ffdir d;
	ffdirentry ent;
	int nfiles = 0;

	FFTEST_FUNC;

	d = ffdir_open(path, pathcap, &ent);
	x(d != NULL);

	for (;;) {
		const ffsyschar *name;
		const fffileinfo *fi;

		if (0 != ffdir_read(d, &ent)) {
			x(fferr_last() == ENOMOREFILES);
			break;
		}

		name = ffdir_entryname(&ent);
		x(name != NULL);

		fi = ffdir_entryinfo(&ent);
		x(fi != NULL);

		if (!ffq_cmp2(name, _S("."))) {
			x(fffile_isdir(fffile_infoattr(fi)));
			nfiles++;
		}
		else if (!ffq_cmp2(name, _S("tmpfile"))) {
			x(!fffile_isdir(fffile_infoattr(fi)));
			x(1 == fffile_infosize(fi));
			nfiles++;
		}
	}
	x(nfiles == 2);

	x(0 == ffdir_close(d));
	return 0;
}

int test_dir(const ffsyschar *tmpdir)
{
	fffd f;
	ffsyschar path[FF_MAXPATH];
	ffsyschar fn[FF_MAXPATH];
	size_t pathcap = FFCNT(path);

	FFTEST_FUNC;

	ffq_cpy2(path, tmpdir);
	ffq_cat2(path, _S("/tmpdir"));

	ffq_cpy2(fn, path);
	ffq_cat2(fn, _S("/tmpfile"));

	x(0 == ffdir_make(path));

	f = fffile_open(fn, FFO_CREATE | O_WRONLY);
	x(f != FF_BADFD);
	x(1 == fffile_write(f, FFSTR("1")));
	x(0 == fffile_close(f));

	test_dirwalk(path, pathcap);
	test_path(path);

	x(0 == fffile_rm(fn));
	x(0 == ffdir_rm(path));
	return 0;
}

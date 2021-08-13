/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/string.h>
#include <FFOS/dir.h>
#include <FFOS/dirscan.h>
#include <FFOS/path.h>
#include <FFOS/file.h>
#include <FFOS/error.h>
#include <FFOS/test.h>
#include <ffbase/stringz.h>


#ifdef FF_UNIX
#define TMP_PATH "/tmp"
#else
#define TMP_PATH "."
#endif

static int test_pathinfo(const char *dirname)
{
	(void)dirname;

	x(ffpath_abs(FFSTR("/absolute/path")));
	x(!ffpath_abs(FFSTR("./relative/path")));
#ifdef FF_WIN
	x(ffpath_abs(FFSTR("\\absolute\\path")));
	x(ffpath_abs(FFSTR("c:\\absolute\\path")));
#endif

#ifdef FF_WIN
	{
		wchar_t wname[256];
		if (0 >= ffsz_utow(wname, FF_COUNT(wname), dirname))
			return -1;
		x(ffpath_islong(wname));
	}
#endif

	ffpathinfo st;
	x_sys(0 == ffpath_infoinit(".", &st));
	x_sys(0 != ffpath_info(&st, FFPATH_BLOCKSIZE));

	return 0;
}

static int test_dirwalk(char *path, ffsize pathcap)
{
	ffdir d;
	ffdirentry ent;
	int nfiles = 0;

	d = ffdir_open(path, pathcap, &ent);
	x(d != NULL);

	for (;;) {
		const char *name;
		const fffileinfo *fi;

		if (0 != ffdir_read(d, &ent)) {
			x(fferr_last() == FFERR_NOMOREFILES);
			break;
		}

		name = ffdir_entry_name(&ent);
		x(name != NULL);

		fi = ffdir_entry_info(&ent);
		x(fi != NULL);

		if (!ffsz_cmp(name, ".")) {
			x(fffile_isdir(fffileinfo_attr(fi)));
			nfiles++;
		}
		else if (!ffsz_cmp(name, "tmpfile")) {
			x(!fffile_isdir(fffileinfo_attr(fi)));
			x(1 == fffileinfo_size(fi));
			nfiles++;
		}
	}
	x(nfiles == 2);

	ffdir_close(d);
	return 0;
}

void test_chdir(const char *dir)
{
#ifdef FF_WIN
	SetCurrentDirectoryA(dir);
#else
	chdir(dir);
#endif
}

void test_dirscan()
{
	const char *name;
	static const char *names[] = {
		"ffostest-dirscan",
			"filea",
			"fileb",
			"filec",
	};
	ffdir_make(names[0]);
	test_chdir(names[0]);
	for (int i = 1;  i != FF_COUNT(names);  i++) {
		fffile_writewhole(names[i], "123", 3, 0);
	}

// default
	ffdirscan d = {};
	x_sys(0 == ffdirscan_open(&d, ".", 0));
	for (int i = 1;  ;  i++) {
		if (NULL == (name = ffdirscan_next(&d))) {
			xieq(i, FF_COUNT(names));
			break;
		}
		xsz(name, names[i]);
	}
	ffdirscan_close(&d);

// wildcard
	d.wildcard = "f*b";
	x_sys(0 == ffdirscan_open(&d, ".", FFDIRSCAN_USEWILDCARD));
	for (;;) {
		if (NULL == (name = ffdirscan_next(&d)))
			break;
		xsz(name, names[2]);
	}
	ffdirscan_close(&d);

// dont skip dot
	x_sys(0 == ffdirscan_open(&d, ".", FFDIRSCAN_DOT));
	for (int i = 1;  ;  i++) {
		if (NULL == (name = ffdirscan_next(&d)))
			break;
		if (i == 1)
			xsz(name, ".");
		else if (i == 2)
			xsz(name, "..");
		else
			break;
	}
	ffdirscan_close(&d);

#ifdef FF_LINUX
// fd
	fffd f = fffile_open(".", FFFILE_READONLY);
	x(f != FFFILE_NULL);
	d.fd = f;
	x_sys(0 == ffdirscan_open(&d, ".", FFDIRSCAN_USEFD));
	for (int i = 1;  ;  i++) {
		if (NULL == (name = ffdirscan_next(&d)))
			break;
		xsz(name, names[i]);
	}
	ffdirscan_close(&d);
#endif

	for (int i = 1;  i != FF_COUNT(names);  i++) {
		fffile_remove(names[i]);
	}
	test_chdir("..");
	ffdir_remove(names[0]);
}

void test_dir()
{
	fffd f;
	char path[256];
	char fn[256];
	ffsize pathcap = FF_COUNT(path);

	strcpy(path, TMP_PATH);
	strcat(path, "/tmpdir");

	strcpy(fn, path);
	strcat(fn, "/tmpfile");

	ffdir_make(path);

	f = fffile_open(fn, FFFILE_CREATE | FFFILE_TRUNCATE | FFFILE_WRITEONLY);
	x(f != FFFILE_NULL);
	x(1 == fffile_write(f, FFSTR("1")));
	x(0 == fffile_close(f));

	test_dirwalk(path, pathcap);
	test_pathinfo(path);

	x(0 == fffile_remove(fn));
	x(0 == ffdir_remove(path));


	strcpy(path, TMP_PATH);
	strcat(path, "/tmpdir/tmpdir2");
	x(0 == ffdir_make_all(path, strlen(TMP_PATH)));
	x(0 == ffdir_remove(path));
	path[strlen(path) - FFS_LEN("/tmpdir2")] = '\0';
	x(0 == ffdir_remove(path));

	test_dirscan();
}

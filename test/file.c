/** ffos: file.h tester
2020, Simon Zolin
*/

#include <FFOS/string.h>
#include <FFOS/file.h>
#include <FFOS/dir.h>
#include <ffbase/stringz.h>
#include <FFOS/test.h>

#ifdef FF_UNIX
#define TMP_PATH "/tmp"
#else
#define TMP_PATH "."
#endif

void test_file_diropen()
{
	char *fn = ffsz_allocfmt("%s/%s", TMP_PATH, "ff.tmp");
	ffdir_remove(fn);

	x(0 == ffdir_make(fn));

	fffd d = fffile_open(fn, FFFILE_READONLY);
	x_sys(d != FFFILE_NULL);
	fffileinfo fi = {};
	x_sys(0 == fffile_info(d, &fi));
	x(fffile_isdir(fffileinfo_attr(&fi)));
	x_sys(0 == fffile_close(d));

	x_sys(0 == ffdir_remove(fn));
	ffmem_free(fn);
}

void test_file_rename()
{
	char *fn = ffsz_allocfmt("%s/%s", TMP_PATH, "ff.tmp");
	char *fnnew = ffsz_allocfmt("%s/%s", TMP_PATH, "ff-new.tmp");
	fffile_remove(fn);
	fffile_remove(fnnew);

	// create
	fffd fd = fffile_open(fn, FFFILE_CREATE | FFFILE_READWRITE);
	x_sys(fd != FFFILE_NULL);
	fffile_close(fd);

	x_sys(fffile_exists(fn));

	x_sys(0 == fffile_rename(fn, fnnew));

	// open - doesn't exist
	fd = fffile_open(fn, FFFILE_READWRITE);
	x(fd == FFFILE_NULL);
	xieq(FFERR_FILENOTFOUND, fferr_last());
	x_sys(!fffile_exists(fn));

	// open
	fd = fffile_open(fnnew, FFFILE_READWRITE);
	x_sys(fd != FFFILE_NULL);
	fffile_close(fd);

	x_sys(0 == fffile_remove(fnnew));
	ffmem_free(fn);
}

/** Check symlink and hardlink */
void test_file_link()
{
	char *fn = ffsz_allocfmt("%s/%s", TMP_PATH, "ff.tmp");
	char *slink = ffsz_allocfmt("%s/%s", TMP_PATH, "ff-slink.tmp");
	char *hlink = ffsz_allocfmt("%s/%s", TMP_PATH, "ff-hlink.tmp");
	fffile_remove(fn);
	fffile_remove(slink);
	fffile_remove(hlink);

	// create
	fffd fd = fffile_open(fn, FFFILE_CREATE | FFFILE_READWRITE);
	x_sys(fd != FFFILE_NULL);
	fffile_close(fd);

#ifdef FF_UNIX
	x_sys(0 == fffile_symlink(fn, slink));
	x_sys(0 == fffile_hardlink(fn, hlink));
#else
	x_sys(0 == fffile_hardlink(fn, hlink) || fferr_last() == 1);
#endif

	fffile_remove(fn);
	fffile_remove(slink);
	fffile_remove(hlink);
	ffmem_free(fn);
	ffmem_free(slink);
	ffmem_free(hlink);
}

/** Check that FFFILE_CREATENEW and FFFILE_CREATE work */
void test_file_create()
{
	fffd fd, fd2;
	char *fn = ffsz_allocfmt("%s/%s", TMP_PATH, "ff.tmp");
	fffile_remove(fn);

	// open - doesn't exist
	fd = fffile_open(fn, 0);
	x(fd == FFFILE_NULL);
	xieq(FFERR_FILENOTFOUND, fferr_last());

	// create new
	fd = fffile_open(fn, FFFILE_CREATENEW | FFFILE_READWRITE);
	x_sys(fd != FFFILE_NULL);

	// create new - already exists
	fd2 = fffile_open(fn, FFFILE_CREATENEW | FFFILE_READWRITE);
	x(fd2 == FFFILE_NULL);
	xieq(FFERR_FILEEXISTS, fferr_last());

	// create - open
	fd2 = fffile_open(fn, FFFILE_CREATE | FFFILE_READWRITE);
	x_sys(fd2 != FFFILE_NULL);
	fffile_close(fd2);

	// open
	fd2 = fffile_open(fn, FFFILE_READWRITE);
	x_sys(fd2 != FFFILE_NULL);
	fffile_close(fd2);

	x_sys(0 == fffile_close(fd));
	x_sys(0 == fffile_remove(fn));

	// create - create
	fd2 = fffile_open(fn, FFFILE_CREATE | FFFILE_READWRITE);
	x(fd2 != FFFILE_NULL);
	fffile_close(fd2);

	x_sys(0 == fffile_remove(fn));
	ffmem_free(fn);
}

/** Check that FFFILE_TRUNCATE works */
void test_file_create_trunc()
{
	char buf[80];
	fffd fd;
	char *fn = ffsz_allocfmt("%s/%s", TMP_PATH, "ff.tmp");
	fffile_remove(fn);

	// create new
	fd = fffile_open(fn, FFFILE_CREATE | FFFILE_READWRITE | FFFILE_TRUNCATE);
	x_sys(fd != FFFILE_NULL);
	x_sys(3 == fffile_write(fd, "123", 3));
	fffile_close(fd);

	// open - truncate
	fd = fffile_open(fn, FFFILE_READWRITE | FFFILE_TRUNCATE);
	x_sys(fd != FFFILE_NULL);
	x_sys(0 == fffile_read(fd, buf, 3));
	fffile_close(fd);

	// create, truncate
	fd = fffile_open(fn, FFFILE_CREATE | FFFILE_READWRITE);
	x_sys(fd != FFFILE_NULL);
	x_sys(3 == fffile_write(fd, "123", 3));
	x_sys(0 == fffile_trunc(fd, 2));
	fffile_close(fd);

	// open, read
	fd = fffile_open(fn, FFFILE_READWRITE);
	x_sys(fd != FFFILE_NULL);
	x_sys(2 == fffile_read(fd, buf, sizeof(buf)));
	x(!ffs_cmpz(buf, 2, "12"));
	fffile_close(fd);

	x_sys(0 == fffile_remove(fn));
	ffmem_free(fn);
}

/** Check that FFFILE_APPEND works */
void test_file_append()
{
	char buf[80];
	fffd fd;
	char *fn = ffsz_allocfmt("%s/%s", TMP_PATH, "ff.tmp");
	fffile_remove(fn);

	// create new
	fd = fffile_open(fn, FFFILE_CREATE | FFFILE_READWRITE | FFFILE_APPEND);
	x_sys(fd != FFFILE_NULL);
	xint_sys(3, fffile_write(fd, "123", 3));
	fffile_close(fd);

	// create
	fd = fffile_open(fn, FFFILE_CREATE | FFFILE_READWRITE | FFFILE_APPEND);
	x_sys(fd != FFFILE_NULL);
	x_sys(3 == fffile_write(fd, "456", 3));
	fffile_close(fd);

	// open
	fd = fffile_open(fn, FFFILE_READWRITE | FFFILE_APPEND);
	x_sys(fd != FFFILE_NULL);
	x_sys(3 == fffile_write(fd, "789", 3));
	x_sys(1 == fffile_seek(fd, 1, FFFILE_SEEK_BEGIN));
	x_sys(1 == fffile_write(fd, "0", 1));

	// open #2
	fffd fd2 = fffile_open(fn, FFFILE_READWRITE | FFFILE_APPEND);
	x_sys(fd2 != FFFILE_NULL);
	x_sys(3 == fffile_write(fd, "abc", 3));

	// check
	xint_sys(13, fffile_readat(fd, buf, sizeof(buf), 0));
	x(!ffs_cmpz(buf, 13, "1234567890abc"));

	// check #2
	xint_sys(13, fffile_readat(fd2, buf, sizeof(buf), 0));
	x(!ffs_cmpz(buf, 13, "1234567890abc"));

	fffile_close(fd);
	fffile_close(fd2);

	x_sys(0 == fffile_remove(fn));
	ffmem_free(fn);
}

void test_file_io()
{
	char buf[80];
	char *fn = ffsz_allocfmt("%s/%s", TMP_PATH, "ff.tmp");
	fffile_remove(fn);

	// create
	fffd fd = fffile_open(fn, FFFILE_CREATE | FFFILE_READWRITE);
	x_sys(fd != FFFILE_NULL);

	// duplicate
	fffd fd2 = fffile_dup(fd);
	x_sys(fd2 != FFFILE_NULL);
	fffile_close(fd);
	fd = fd2;

	// write/read
	x_sys(3 == fffile_write(fd, "123", 3));
	x_sys(0 == fffile_read(fd, buf, sizeof(buf)));
	x_sys(1 == fffile_seek(fd, 1, FFFILE_SEEK_BEGIN));
	x_sys(2 == fffile_read(fd, buf, sizeof(buf)));
	x(!ffs_cmpz(buf, 2, "23"));
	x_sys(0 == fffile_read(fd, buf, sizeof(buf)));

	// writeat/readat
	x_sys(3 == fffile_writeat(fd, "abc", 3, 0));
	x_sys(2 == fffile_readat(fd, buf, sizeof(buf), 1));
	x(!ffs_cmpz(buf, 2, "bc"));

	x_sys(0 == fffile_close(fd));

	x_sys(0 == fffile_remove(fn));
	ffmem_free(fn);
}

/** Check get/set file properties */
void test_file_info()
{
	fffd fd;
	char *fn = ffsz_allocfmt("%s/%s", TMP_PATH, "ff.tmp");
	fffile_remove(fn);

	// create
	fd = fffile_open(fn, FFFILE_CREATE | FFFILE_READWRITE);
	x_sys(fd != FFFILE_NULL);
	x_sys(3 == fffile_write(fd, "123", 3));

	// get info
	fffileinfo fi = {};
	x_sys(0 == fffile_info(fd, &fi));

	xieq(3, fffileinfo_size(&fi));
	xieq(3, fffile_size(fd));

	x(!fffile_isdir(fffileinfo_attr(&fi)));
#ifdef FF_WIN
	x(fffileinfo_attr(&fi) == FILE_ATTRIBUTE_NORMAL
		|| fffileinfo_attr(&fi) == FILE_ATTRIBUTE_ARCHIVE);
#else
	xieq(FFFILE_UNIX_REG, fffileinfo_attr(&fi) & FFFILE_UNIX_TYPEMASK);
	x(0 != fffileinfo_owner(&fi));
	x_sys(0 == fffile_set_owner(fd, fi.st_uid, fi.st_gid));
#endif

	x(0 != fffileinfo_id(&fi));
	x(0 != fffileinfo_mtime(&fi).sec);

	// set properties
#if !defined FF_WIN || FF_WIN >= 0x0600
	x_sys(0 == fffile_set_attr(fd, fffileinfo_attr(&fi)));
#endif
	fftime mtime = fffileinfo_mtime(&fi);
	x_sys(0 == fffile_set_mtime(fd, &mtime));

	fffile_close(fd);

	// set properties by name
	x_sys(0 == fffile_set_attr_path(fn, fffileinfo_attr(&fi)));
	x_sys(0 == fffile_set_mtime_path(fn, &mtime));

	// get info by name
	x_sys(0 == fffile_info_path(fn, &fi));

	x_sys(0 == fffile_remove(fn));
	ffmem_free(fn);
}

void test_file_temp()
{
	char *fn = ffsz_allocfmt("%s/%s", TMP_PATH, "ff.tmp");
	fffile_remove(fn);

	fffd fd;
	fd = fffile_createtemp(fn, FFFILE_READWRITE);
	x_sys(fd != FFFILE_NULL);
	x_sys(0 == fffile_close(fd));

	x(FFFILE_NULL == fffile_open(fn, FFFILE_READWRITE));
	xieq(FFERR_FILENOTFOUND, fferr_last());

	ffmem_free(fn);
}

void test_file_winattr()
{
#ifdef FF_WIN
	fffd fd;
	char *fn = ffsz_allocfmt("%s/%s", TMP_PATH, "ff.tmp");
	fffile_remove(fn);

	// create
	fd = fffile_open(fn, FFFILE_CREATE | FFFILE_READWRITE | FILE_ATTRIBUTE_READONLY);
	x_sys(fd != FFFILE_NULL);
	x_sys(3 == fffile_write(fd, "123", 3));
	fffile_close(fd);

	// check
	fffileinfo fi = {};
	x(0 == fffile_info_path(fn, &fi));
	x(0 != (fffileinfo_attr(&fi) & FILE_ATTRIBUTE_READONLY));
	x(0 == (fffileinfo_attr(&fi) & FILE_ATTRIBUTE_NORMAL));

	x_sys(0 == fffile_remove(fn));
	ffmem_free(fn);
#endif
}

void test_file_dosname()
{
#ifdef FF_WIN
	fffd fd;
	char *fn = ffsz_allocfmt("%s/%s", TMP_PATH, "somelonglongname.tmplong");
	fffile_remove(fn);

	// create
	fd = fffile_open(fn, FFFILE_CREATE | FFFILE_READWRITE);
	x_sys(fd != FFFILE_NULL);
	x_sys(3 == fffile_write(fd, "123", 3));
	fffile_close(fd);

	char *fn_dos = ffsz_allocfmt("%s/%s", TMP_PATH, "somelo~1.tmp");

	// open
	fd = fffile_open(fn_dos, FFFILE_READWRITE);
	x_sys(fd != FFFILE_NULL);
	fffile_close(fd);

	// open - fail
	fd = fffile_open(fn_dos, FFFILE_READWRITE | FFFILE_NODOSNAME);
	x(fd == FFFILE_NULL);
	fffile_close(fd);

	x_sys(0 == fffile_remove(fn));
	ffmem_free(fn);
	ffmem_free(fn_dos);
#endif
}

void test_file_mount()
{
#ifdef FF_WIN
	fffd hvol;
	wchar_t buf[512];
	x_sys(FFFILE_NULL != (hvol = FindFirstVolumeW((void*)buf, sizeof(buf))));
	char *volname = ffsz_alloc_wtou(buf);
	const char *dirname = "ffos-mountdir";
	(void) ffdir_make(dirname);
	x_sys(0 == fffile_mount(volname, dirname));
	x_sys(0 == fffile_mount(NULL, dirname));
	(void) ffdir_remove(dirname);
	ffmem_free(volname);
	FindVolumeClose(hvol);
#endif
}

void test_file_rwwhole()
{
	char *fn = ffsz_allocfmt("%s/%s", TMP_PATH, "whole.tmp");
	x(0 == fffile_writewhole(fn, "wholedata", 9, 0));
	ffvec data = {};
	x(0 == fffile_readwhole(fn, &data, -1));
	xseq((ffstr*)&data, "wholedata");
	ffmem_free(fn);
}

void test_file()
{
	test_file_create();
	test_file_create_trunc();
	test_file_diropen();
	test_file_append();
	test_file_io();
	test_file_info();
	test_file_temp();
	test_file_winattr();
	test_file_dosname();
	test_file_link();
	test_file_rename();
	test_file_mount();
	test_file_rwwhole();
}

/**
Copyright (c) 2013 Simon Zolin
*/

#include "all.h"
#include <FFOS/string.h>
#include <FFOS/file.h>
#include <FFOS/dir.h>
#include <FFOS/error.h>
#include <FFOS/test.h>
#include <FFOS/asyncio.h>
#include <ffbase/stringz.h>
#include <test/test.h>

#define HELLO "hello\n"
#define FOOBAR "foobar\n"

void test_diropen()
{
	char *fn = ffsz_allocfmt("%s/%s", TMP_PATH, "ff.tmp");
	ffdir_rm(fn);

	x(0 == ffdir_make(fn));

	fffd d = fffile_open(fn, FFFILE_READONLY);
	x_sys(d != FF_BADFD);
	fffileinfo fi = {};
	x_sys(0 == fffile_info(d, &fi));
	x(fffile_isdir(fffile_infoattr(&fi)));
	x_sys(0 == fffile_close(d));

	x_sys(0 == ffdir_rm(fn));
	ffmem_free(fn);
}

static int test_std()
{
	FFTEST_FUNC;

	x(FFSLEN(HELLO) == fffile_write(ffstdout, FFSTR(HELLO)));
	x(FFSLEN(HELLO) == ffstd_write(ffstdout, FFSTR(HELLO)));
	return 0;
}

static int test_pipe()
{
	char buf[64];
	fffd rd = FF_BADFD
		, wr = FF_BADFD;

	FFTEST_FUNC;

	x(0 == ffpipe_create2(&rd, &wr, FFO_NONBLOCK));
	x(-1 == ffpipe_read(rd, buf, FFCNT(buf)) && fferr_again(fferr_last()));
	x(0 == ffpipe_nblock(rd, 0));

	x(FFSLEN(HELLO) == fffile_write(wr, FFSTR(HELLO)));
	x(FFSLEN(HELLO) == ffpipe_read(rd, buf, FFCNT(buf)));
	x(!memcmp(buf, FFSTR(HELLO)));

	x(0 == ffpipe_close(rd));
	x(0 == ffpipe_close(wr));
	return 0;
}

void test_file_rename()
{
	char *fn = ffsz_allocfmt("%s/%s", TMP_PATH, "ff.tmp");
	char *fnnew = ffsz_allocfmt("%s/%s", TMP_PATH, "ff-new.tmp");
	fffile_remove(fn);
	fffile_remove(fnnew);

	// create
	fffd fd = fffile_open(fn, FFFILE_CREATE | FFFILE_READWRITE);
	x_sys(fd != FF_BADFD);
	fffile_close(fd);

	x_sys(0 == fffile_rename(fn, fnnew));

	// open - doesn't exist
	fd = fffile_open(fn, FFFILE_READWRITE);
	x(fd == FF_BADFD);
	xieq(FFERR_FILENOTFOUND, fferr_last());

	// open
	fd = fffile_open(fnnew, FFFILE_READWRITE);
	x_sys(fd != FF_BADFD);
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
	x_sys(fd != FF_BADFD);
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
	x(fd == FFFILE_BAD);
	xieq(FFERR_FILENOTFOUND, fferr_last());

	// create new
	fd = fffile_open(fn, FFFILE_CREATENEW | FFFILE_READWRITE);
	x_sys(fd != FFFILE_BAD);

	// create new - already exists
	fd2 = fffile_open(fn, FFFILE_CREATENEW | FFFILE_READWRITE);
	x(fd2 == FFFILE_BAD);
	xieq(FFERR_FILEEXISTS, fferr_last());

	// create - open
	fd2 = fffile_open(fn, FFFILE_CREATE | FFFILE_READWRITE);
	x_sys(fd2 != FFFILE_BAD);
	fffile_close(fd2);

	// open
	fd2 = fffile_open(fn, FFFILE_READWRITE);
	x_sys(fd2 != FFFILE_BAD);
	fffile_close(fd2);

	x_sys(0 == fffile_close(fd));
	x_sys(0 == fffile_remove(fn));

	// create - create
	fd2 = fffile_open(fn, FFFILE_CREATE | FFFILE_READWRITE);
	x(fd2 != FFFILE_BAD);
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
	x_sys(fd != FFFILE_BAD);
	x_sys(3 == fffile_write(fd, "123", 3));
	fffile_close(fd);

	// open - truncate
	fd = fffile_open(fn, FFFILE_READWRITE | FFFILE_TRUNCATE);
	x_sys(fd != FFFILE_BAD);
	x_sys(0 == fffile_read(fd, buf, 3));
	fffile_close(fd);

	// create, truncate
	fd = fffile_open(fn, FFFILE_CREATE | FFFILE_READWRITE);
	x_sys(fd != FFFILE_BAD);
	x_sys(3 == fffile_write(fd, "123", 3));
	x_sys(0 == fffile_trunc(fd, 2));
	fffile_close(fd);

	// open, read
	fd = fffile_open(fn, FFFILE_READWRITE);
	x_sys(fd != FFFILE_BAD);
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
	x_sys(fd != FFFILE_BAD);
	xieq_sys(3, fffile_write(fd, "123", 3));
	fffile_close(fd);

	// create
	fd = fffile_open(fn, FFFILE_CREATE | FFFILE_READWRITE | FFFILE_APPEND);
	x_sys(fd != FFFILE_BAD);
	x_sys(3 == fffile_write(fd, "456", 3));
	fffile_close(fd);

	// open
	fd = fffile_open(fn, FFFILE_READWRITE | FFFILE_APPEND);
	x_sys(fd != FFFILE_BAD);
	x_sys(3 == fffile_write(fd, "789", 3));
	x_sys(1 == fffile_seek(fd, 1, FFFILE_SEEK_BEGIN));
	x_sys(1 == fffile_write(fd, "0", 1));

	// check
	xieq_sys(10, fffile_readat(fd, buf, sizeof(buf), 0));
	x(!ffs_cmpz(buf, 10, "1234567890"));
	fffile_close(fd);

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
	x_sys(fd != FFFILE_BAD);

	// duplicate
	fffd fd2 = fffile_dup(fd);
	x_sys(fd2 != FFFILE_BAD);
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
	x_sys(fd != FFFILE_BAD);
	x_sys(3 == fffile_write(fd, "123", 3));

	// get info
	fffileinfo fi = {};
	x_sys(0 == fffile_info(fd, &fi));

	xieq(3, fffileinfo_size(&fi));
	xieq(3, fffile_size(fd));

	x(!fffile_isdir(fffileinfo_attr(&fi)));
#ifdef FF_WIN
	xieq(FILE_ATTRIBUTE_ARCHIVE, fffileinfo_attr(&fi));
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
	x_sys(fd != FF_BADFD);
	x_sys(0 == fffile_close(fd));

	x(FF_BADFD == fffile_open(fn, FFFILE_READWRITE));
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
	x_sys(fd != FFFILE_BAD);
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
	x_sys(fd != FFFILE_BAD);
	x_sys(3 == fffile_write(fd, "123", 3));
	fffile_close(fd);

	char *fn_dos = ffsz_allocfmt("%s/%s", TMP_PATH, "somelo~1.tmp");

	// open
	fd = fffile_open(fn_dos, FFFILE_READWRITE);
	x_sys(fd != FFFILE_BAD);
	fffile_close(fd);

	// open - fail
	fd = fffile_open(fn_dos, FFFILE_READWRITE | FFFILE_NODOSNAME);
	x(fd == FFFILE_BAD);
	fffile_close(fd);

	x_sys(0 == fffile_remove(fn));
	ffmem_free(fn);
	ffmem_free(fn_dos);
#endif
}

int test_file()
{
	FFTEST_FUNC;

	test_file_create();
	test_file_create_trunc();
	test_file_append();
	test_file_io();
	test_file_info();
	test_file_temp();
	test_file_winattr();
	test_file_dosname();
	test_file_link();
	test_file_rename();
	test_diropen();
	test_std();
	test_pipe();
	return 0;
}


struct aio_test {
	char *buf;
	size_t len;
	uint64 bsize;
	uint ok;
	fffd kq;
	ffkqu_time kqtm;
};

static struct aio_test faio;

static void test_onwrite(void *udata)
{
	ffaio_filetask *ft = udata;
	ssize_t r;

	FFTEST_TIMECALL(r = ffaio_fwrite(ft, faio.buf, faio.len, 0, &test_onwrite));

	if (r == -1 && fferr_again(fferr_last())) {
		ffkqu_entry ent;
		fffile_writecz(ffstdout, "ffkqu_wait...");
		FFTEST_TIMECALL(x(1 == ffkqu_wait(faio.kq, &ent, 1, &faio.kqtm)));
		ffkev_call(&ent);
		return;
	}

	if (r == -1) {
		x(0);
		return;
	}

	x(r == faio.len);
	faio.ok++;
}

static void test_onread(void *udata)
{
	ffaio_filetask *ft = udata;
	ssize_t r;

	FFTEST_TIMECALL(r = ffaio_fread(ft, faio.buf, faio.len, faio.bsize, &test_onread));

	if (r == -1 && fferr_again(fferr_last())) {
		ffkqu_entry ent;
		fffile_writecz(ffstdout, "ffkqu_wait...");
		FFTEST_TIMECALL(x(1 == ffkqu_wait(faio.kq, &ent, 1, &faio.kqtm)));
		ffkev_call(&ent);
		return;
	}

	if (r == -1) {
		x(0);
		return;
	}

	x(r == faio.len - faio.bsize - 1);
	x(faio.buf[0] == '!' && faio.buf[r - 1] == '!');
	faio.ok++;
}

int test_fileaio(void)
{
	fffd f;
	ffaio_filetask tfil;
	const char *fn = "./ff-aiofile";
	FFTEST_FUNC;

	{
	ffpathinfo fsst;
	x(0 == ffpath_infoinit(".", &fsst));
	faio.bsize = ffpath_info(&fsst, FFPATH_BSIZE);
	}

	uint flags = 0;
	flags |= FFO_DIRECT;
	x(FF_BADFD != (f = fffile_open(fn, O_CREAT | O_RDWR | flags)));
	x(FF_BADFD != (faio.kq = ffkqu_create()));
	ffkqu_settm(&faio.kqtm, -1);

	x(0 == ffaio_fctxinit());

	faio.len = 2 * faio.bsize;
	x(NULL != (faio.buf = ffmem_align(faio.len, faio.bsize)));
	memset(faio.buf, '!', faio.len);
	x(0 == fffile_trunc(f, faio.len));

	ffaio_finit(&tfil, f, &tfil);
	x(0 == ffaio_fattach(&tfil, faio.kq, !!(flags & FFO_DIRECT)));

//write
	fffile_writecz(ffstdout, "ffaio_fwrite...");
	test_onwrite(&tfil);
	x(faio.ok == 1);

//overwrite
	fffile_writecz(ffstdout, "ffaio_fwrite...");
	test_onwrite(&tfil);
	x(faio.ok == 2);

//read
	fffd f2;
	x(FF_BADFD != (f2 = fffile_open(fn, O_RDWR)));
	x(0 == fffile_trunc(f2, faio.len - 1));
	fffile_close(f2);

	memset(faio.buf, '\0', faio.len);

	fffile_writecz(ffstdout, "ffaio_fread...");
	test_onread(&tfil);
	x(faio.ok == 3);

	ffaio_fctxclose();
	ffmem_alignfree(faio.buf);
	fffile_close(f);
	ffkqu_close(faio.kq);
	fffile_remove(fn);

	return 0;
}

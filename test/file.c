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

#define x FFTEST_BOOL
#define HELLO "hello\n"
#define FOOBAR "foobar\n"

static int test_diropen(const char *tmpdir)
{
	fffd hdir;
	char fn[FF_MAXPATH];
	fffileinfo fi;

	FFTEST_FUNC;

	strcpy(fn, tmpdir);
	strcat(fn, "/tmpdir");

	x(0 == ffdir_make(fn));

	hdir = fffile_open(fn, O_RDONLY);
	x(hdir != FF_BADFD);
	x(0 == fffile_info(hdir, &fi));
	x(fffile_isdir(fffile_infoattr(&fi)));
	x(0 == fffile_close(hdir));

	x(0 == ffdir_rm(fn));

	return 0;
}

static int test_std()
{
	FFTEST_FUNC;

	x(FFSLEN(HELLO) == fffile_write(ffstdout, FFSTR(HELLO)));
	x(FFSLEN(HELLO) == ffstd_write(ffstdout, FFSTR(HELLO)));
	return 0;
}

static int test_mapwr(const char *fn)
{
	fffd fd
		, fmap;
	void *mapd;
	size_t mapsz;

	FFTEST_FUNC;

	fd = fffile_open(fn, O_RDWR);
	x(fd != FF_BADFD);

	mapsz = 64*1024 + FFSLEN(FOOBAR);
	x(0 == fffile_trunc(fd, mapsz));
	fmap = ffmap_create(fd, mapsz, FFMAP_PAGERW);
	x(fmap != 0);

	mapd = ffmap_open(fmap, 0, mapsz, PROT_READ | PROT_WRITE, MAP_SHARED);
	x(mapd != NULL);
	x(!memcmp(mapd, FFSTR(HELLO)));
	memcpy((char*)mapd + 64*1024, FOOBAR, FFSLEN(FOOBAR));
	x(0 == ffmap_unmap(mapd, mapsz));

	mapsz = FFSLEN(FOOBAR);
	mapd = ffmap_open(fmap, 64*1024, mapsz, PROT_READ | PROT_WRITE, MAP_SHARED);
	x(mapd != NULL);
	x(mapsz == FFSLEN(FOOBAR) && !memcmp(mapd, FFSTR(FOOBAR)));
	x(0 == ffmap_unmap(mapd, mapsz));

	ffmap_close(fmap);
	x(0 == fffile_close(fd));
	return 0;
}

static int test_mapanon()
{
	fffd fmap;
	void *mapd;
	size_t mapsz = 64*1024;

	FFTEST_FUNC;

	fmap = ffmap_create(FF_BADFD, mapsz, FFMAP_PAGERW);
	x(fmap != 0);
	mapd = ffmap_open(fmap, 0, mapsz, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS);
	x(mapd != NULL);

	memcpy(mapd, HELLO, FFSLEN(HELLO));
	x(!memcmp(mapd, FFSTR(HELLO)));

	x(0 == ffmap_unmap(mapd, mapsz));
	ffmap_close(fmap);

	return 0;
}

static int test_map(const char *fn)
{
	fffd fd
		, fmap;
	uint64 fsiz;
	void *mapd;
	size_t mapsz;

	FFTEST_FUNC;

	fd = fffile_open(fn, O_RDONLY);
	x(fd != FF_BADFD);
	fsiz = fffile_size(fd);

	fmap = ffmap_create(fd, fsiz, FFMAP_PAGEREAD);
	x(fmap != 0);

	mapsz = (size_t)fsiz;
	mapd = ffmap_open(fmap, 0, mapsz, PROT_READ, MAP_SHARED);
	x(mapd != NULL);

	x(mapsz == FFSLEN(HELLO) && !memcmp(mapd, FFSTR(HELLO)));

	x(0 == ffmap_unmap(mapd, mapsz));
	ffmap_close(fmap);
	x(0 == fffile_close(fd));

	test_mapwr(fn);
	test_mapanon();

	return 0;
}

static int test_pipe()
{
	char buf[64];
	fffd rd = FF_BADFD
		, wr = FF_BADFD;

	FFTEST_FUNC;

	x(0 == ffpipe_create(&rd, &wr));

#ifdef FF_UNIX
	x(0 == fffile_nblock(rd, 1));
	x(-1 == fffile_read(rd, buf, FFCNT(buf)) && fferr_again(fferr_last()));
	x(0 == fffile_nblock(rd, 0));
#endif

	x(FFSLEN(HELLO) == fffile_write(wr, FFSTR(HELLO)));
	x(FFSLEN(HELLO) == fffile_read(rd, buf, FFCNT(buf)));
	x(!memcmp(buf, FFSTR(HELLO)));

	x(0 == ffpipe_close(rd));
	x(0 == ffpipe_close(wr));
	return 0;
}

static int test_fileinfo(char *fn)
{
	fffileinfo fi;
	fffd fd;
	fftime lw;

	FFTEST_FUNC;

	fd = fffile_open(fn, O_RDONLY);
	x(fd != FF_BADFD);

	x(0 == fffile_info(fd, &fi));
	x(0 != fffile_infoattr(&fi));
	x(0 != fffile_infosize(&fi));
	x(0 != fffile_infoid(&fi));
	lw = fffile_infomtime(&fi);
	x(fftime_sec(&lw) != 0);

	x(0 == fffile_close(fd));
	return 0;
}

static void file_link(void)
{
	const char *fn = "./ff-file", *slink = "./ff-slink", *hlink = "./ff-hlink";
	fffd f;
	x(FF_BADFD != (f = fffile_open(fn, O_CREAT | O_WRONLY)));
	fffile_close(f);

#ifdef FF_UNIX
	x(0 == fffile_symlink(fn, slink));
	x(0 == fffile_hardlink(fn, hlink));
#else
	x(0 == fffile_hardlink(fn, hlink) || fferr_last() == 1);
#endif

	fffile_rm(fn);
	fffile_rm(slink);
	fffile_rm(hlink);
}

int test_file()
{
	const char *tmpdir = TMP_PATH;
	fffd fd;
	char fn[FF_MAXPATH];
	char buf[4096];

	FFTEST_FUNC;

	strcpy(fn, tmpdir);
	strcat(fn, "/tmpfile");

	fd = fffile_open(fn, O_CREAT | O_TRUNC | O_RDWR);
	x(fd != FF_BADFD);
	x(FFSLEN(HELLO) == fffile_write(fd, FFSTR(HELLO)));

	x(0 == fffile_seek(fd, 0, SEEK_SET));
	x(FFSLEN(HELLO) == fffile_read(fd, buf, sizeof(buf)));
	x(0 == fffile_close(fd));

	test_fileinfo(fn);
	test_map(fn);

	x(0 == fffile_rm(fn));

	fd = fffile_createtemp(fn, O_WRONLY);
	x(fd != FF_BADFD);
	x(0 == fffile_close(fd));
	x(FF_BADFD == fffile_open(fn, O_RDONLY));

	test_diropen(tmpdir);
	test_std();
	test_pipe();
	file_link();

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
	flags |= O_DIRECT;
	x(FF_BADFD != (f = fffile_open(fn, O_CREAT | O_RDWR | flags)));
	x(FF_BADFD != (faio.kq = ffkqu_create()));
	ffkqu_settm(&faio.kqtm, -1);

	x(0 == ffaio_fctxinit());

	faio.len = 2 * faio.bsize;
	x(NULL != (faio.buf = ffmem_align(faio.len, faio.bsize)));
	memset(faio.buf, '!', faio.len);
	x(0 == fffile_trunc(f, faio.len));

	ffaio_finit(&tfil, f, &tfil);
	x(0 == ffaio_fattach(&tfil, faio.kq, !!(flags & O_DIRECT)));

//write
	fffile_writecz(ffstdout, "ffaio_fwrite...");
	test_onwrite(&tfil);
	x(faio.ok == 1);

//overwrite
	fffile_writecz(ffstdout, "ffaio_fwrite...");
	test_onwrite(&tfil);
	x(faio.ok == 2);

//read
#ifdef FF_WIN
	fffd f2;
	x(FF_BADFD != (f2 = fffile_open(fn, O_RDWR)));
	x(0 == fffile_trunc(f2, faio.len - 1));
	fffile_close(f2);

#else
	x(0 == fffile_trunc(f, faio.len - 1));
#endif
	memset(faio.buf, '\0', faio.len);

	fffile_writecz(ffstdout, "ffaio_fread...");
	test_onread(&tfil);
	x(faio.ok == 3);

	ffaio_fctxclose();
	ffmem_alignfree(faio.buf);
	fffile_close(f);
	ffkqu_close(faio.kq);
	fffile_rm(fn);

	return 0;
}

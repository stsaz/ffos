/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/string.h>
#include <FFOS/file.h>
#include <FFOS/dir.h>
#include <FFOS/error.h>
#include <FFOS/test.h>

#define x FFTEST_BOOL
#define HELLO "hello\n"
#define FOOBAR "foobar\n"

static int test_diropen(const ffsyschar *tmpdir)
{
	fffd hdir;
	ffsyschar fn[FF_MAXPATH];

	FFTEST_FUNC;

	ffq_cpy2(fn, tmpdir);
	ffq_cat2(fn, TEXT("/tmpdir"));

	x(0 == ffdir_make(fn));

	hdir = fffile_open(fn, FFO_OPEN | O_RDONLY | O_DIR);
	x(hdir != FF_BADFD);
	x(fffile_isdir(fffile_attr(hdir)));
	x(fffile_isdir(fffile_attrfn(fn)));
	x(0 == fffile_close(hdir));

	x(ffdir_exists(fn));
	x(0 == ffdir_rm(fn));
	x(!ffdir_exists(fn));

	return 0;
}

static int test_std()
{
	FFTEST_FUNC;

	x(FFSLEN(HELLO) == fffile_write(ffstdout, FFSTR(HELLO)));
	x(FFSLEN(HELLO) == ffstd_write(ffstdout, FFSTRQ(HELLO)));
	return 0;
}

static int test_mapwr(const ffsyschar *fn)
{
	fffd fd
		, fmap;
	void *mapd;
	size_t mapsz;

	FFTEST_FUNC;

	fd = fffile_open(fn, FFO_OPEN | O_RDWR);
	x(fd != FF_BADFD);

	mapsz = 64*1024 + FFSLEN(FOOBAR);
	x(0 == fffile_trunc(fd, mapsz));
	fmap = ffmap_create(fd, mapsz, FFMAP_PAGERW);
	x(fmap != 0);

	mapd = ffmap_open(fmap, 0, mapsz, PROT_READ | PROT_WRITE, MAP_SHARED);
	x(mapd != NULL);
	x(!strncmp(mapd, FFSTR(HELLO)));
	memcpy((char*)mapd + 64*1024, FFSTR(FOOBAR));
	x(0 == ffmap_unmap(mapd, mapsz));

	mapsz = FFSLEN(FOOBAR);
	mapd = ffmap_open(fmap, 64*1024, mapsz, PROT_READ | PROT_WRITE, MAP_SHARED);
	x(mapd != NULL);
	x(mapsz == FFSLEN(FOOBAR) && !strncmp(mapd, FFSTR(FOOBAR)));
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

	memcpy(mapd, FFSTR(HELLO));
	x(!strncmp(mapd, FFSTR(HELLO)));

	x(0 == ffmap_unmap(mapd, mapsz));
	ffmap_close(fmap);

	return 0;
}

static int test_map(const ffsyschar *fn)
{
	fffd fd
		, fmap;
	uint64 fsiz;
	void *mapd;
	size_t mapsz;

	FFTEST_FUNC;

	fd = fffile_open(fn, FFO_OPEN | O_RDONLY);
	x(fd != FF_BADFD);
	fsiz = fffile_size(fd);

	fmap = ffmap_create(fd, fsiz, FFMAP_PAGEREAD);
	x(fmap != 0);

	mapsz = (size_t)fsiz;
	mapd = ffmap_open(fmap, 0, mapsz, PROT_READ, MAP_SHARED);
	x(mapd != NULL);

	x(mapsz == FFSLEN(HELLO) && !strncmp(mapd, FFSTR(HELLO)));

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
	fffd rd, wr;

	FFTEST_FUNC;

	x(0 == ffpipe_create(&rd, &wr));

#ifdef FF_UNIX
	x(0 == fffile_nblock(rd, 1));
	x(-1 == fffile_read(rd, buf, FFCNT(buf)) && fferr_again(fferr_last()));
	x(0 == fffile_nblock(rd, 0));
#endif

	x(FFSLEN(HELLO) == fffile_write(wr, FFSTR(HELLO)));
	x(FFSLEN(HELLO) == fffile_read(rd, buf, FFCNT(buf)));
	x(!strncmp(buf, FFSTR(HELLO)));

	x(0 == ffpipe_close(rd));
	x(0 == ffpipe_close(wr));
	return 0;
}

static int test_fileinfo(ffsyschar *fn)
{
	fffileinfo fi;
	fffd fd;

	FFTEST_FUNC;

	fd = fffile_open(fn, FFO_OPEN | O_RDONLY);
	x(fd != FF_BADFD);

	x(fffile_attrfn(fn) == fffile_attr(fd));

	x(0 == fffile_info(fd, &fi));
	x(fffile_infoattr(&fi) == fffile_attr(fd));
	x(fffile_infosize(&fi) == fffile_size(fd));

	{
		fftime lw, la, cre;
		fftime lw2;
		x(0 == fffile_time(fd, &lw, &la, &cre));
		lw2 = fffile_infotimew(&fi);
		x(0 == fftime_cmp(&lw, &lw2));
	}

	x(0 == fffile_close(fd));
	return 0;
}

int test_file(const ffsyschar *tmpdir)
{
	fffd fd;
	ffsyschar fn[FF_MAXPATH];
	char buf[4096];

	FFTEST_FUNC;

	ffq_cpy2(fn, tmpdir);
	ffq_cat2(fn, TEXT("/tmpfile"));

	fd = fffile_open(fn, FFO_CREATE | O_RDWR);
	x(fd != FF_BADFD);
	x(FFSLEN(HELLO) == fffile_write(fd, FFSTR(HELLO)));

	x(0 == fffile_seek(fd, 0, SEEK_SET));
	x(FFSLEN(HELLO) == fffile_read(fd, buf, sizeof(buf)));
	x(0 == fffile_close(fd));

	test_fileinfo(fn);
	test_map(fn);

	x(fffile_exists(fn));
	x(0 == fffile_rm(fn));
	x(!fffile_exists(fn));

	test_diropen(tmpdir);
	test_std();
	test_pipe();

	return 0;
}

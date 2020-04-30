/** File mapping tests.
Copyright (c) 2020 Simon Zolin
*/

#include "all.h"
#include <FFOS/string.h>
#include <FFOS/file.h>
#include <FFOS/error.h>
#include <FFOS/test.h>

#define x FFTEST_BOOL
#define HELLO "hello\n"
#define FOOBAR "foobar\n"

static int test_mapwr(const char *fn)
{
	fffd fd
		, fmap;
	void *mapd;
	size_t mapsz;

	FFTEST_FUNC;

	fd = fffile_open(fn, FFO_CREATE | FFO_RDWR);
	x(fd != FF_BADFD);

	mapsz = 64*1024 + FFSLEN(FOOBAR);
	x(0 == fffile_trunc(fd, mapsz));
	fmap = ffmap_create(fd, mapsz, FFMAP_PAGERW);
	x(fmap != 0);

	mapd = ffmap_open(fmap, 0, mapsz, PROT_READ | PROT_WRITE, MAP_SHARED);
	x(mapd != NULL);
	x(!memcmp(mapd, FFSTR("\x00\x00")));
	x(!memcmp(mapd + 64*1024, FFSTR("\x00\x00")));
	memcpy((char*)mapd, HELLO, FFSLEN(HELLO));
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

static int test_mapro(const char *fn)
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

	x(mapsz == 64*1024 + FFSLEN(FOOBAR));
	x(!memcmp(mapd, FFSTR(HELLO)));
	x(!memcmp(mapd + 64*1024, FFSTR(FOOBAR)));

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

int test_filemap()
{
	FFTEST_FUNC;

	char fn[FF_MAXPATH];
	strcpy(fn, TMP_PATH);
	strcat(fn, "/tmpfile");

	fffile_rm(fn);
	test_mapwr(fn);
	test_mapro(fn);
	test_mapanon();

	fffile_rm(fn);
	return 0;
}

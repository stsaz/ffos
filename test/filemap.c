/** ffos: filemap.h tester
2020, Simon Zolin
*/

#include <FFOS/filemap.h>
#include <FFOS/file.h>
#include <FFOS/string.h>
#include <FFOS/error.h>
#include <FFOS/test.h>


#define HELLO "hello\n"
#define FOOBAR "foobar\n"

#ifdef FF_UNIX
#define TMP_PATH "/tmp"
#else
#define TMP_PATH "."
#endif

static int test_mapwr(const char *fn)
{
	fffd fd
		, fmap;
	void *mapd;
	size_t mapsz;

	fd = fffile_open(fn, FFFILE_CREATE | FFFILE_READWRITE);
	x(fd != FFFILE_NULL);

	mapsz = 64*1024 + FFS_LEN(FOOBAR);
	x(0 == fffile_trunc(fd, mapsz));
	fmap = ffmmap_create(fd, mapsz, FFMMAP_READWRITE);
	x(fmap != 0);

	mapd = ffmmap_open(fmap, 0, mapsz, PROT_READ | PROT_WRITE, MAP_SHARED);
	x(mapd != NULL);
	x(!memcmp(mapd, FFSTR("\x00\x00")));
	x(!memcmp(mapd + 64*1024, FFSTR("\x00\x00")));
	memcpy((char*)mapd, HELLO, FFS_LEN(HELLO));
	memcpy((char*)mapd + 64*1024, FOOBAR, FFS_LEN(FOOBAR));
	x(0 == ffmmap_unmap(mapd, mapsz));

	mapsz = FFS_LEN(FOOBAR);
	mapd = ffmmap_open(fmap, 64*1024, mapsz, PROT_READ | PROT_WRITE, MAP_SHARED);
	x(mapd != NULL);
	x(mapsz == FFS_LEN(FOOBAR) && !memcmp(mapd, FFSTR(FOOBAR)));
	x(0 == ffmmap_unmap(mapd, mapsz));

	ffmmap_close(fmap);
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

	fd = fffile_open(fn, FFFILE_READONLY);
	x(fd != FFFILE_NULL);
	fsiz = fffile_size(fd);
	fmap = ffmmap_create(fd, fsiz, FFMMAP_READ);
	x(fmap != 0);

	mapsz = (size_t)fsiz;
	mapd = ffmmap_open(fmap, 0, mapsz, PROT_READ, MAP_SHARED);
	x(mapd != NULL);

	x(mapsz == 64*1024 + FFS_LEN(FOOBAR));
	x(!memcmp(mapd, FFSTR(HELLO)));
	x(!memcmp(mapd + 64*1024, FFSTR(FOOBAR)));

	x(0 == ffmmap_unmap(mapd, mapsz));
	ffmmap_close(fmap);

	x(0 == fffile_close(fd));
	return 0;
}

static int test_mapanon()
{
	fffd fmap;
	void *mapd;
	size_t mapsz = 64*1024;

	fmap = ffmmap_create(FFFILE_NULL, mapsz, FFMMAP_READWRITE);
	x(fmap != 0);
	mapd = ffmmap_open(fmap, 0, mapsz, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS);
	x(mapd != NULL);

	memcpy(mapd, HELLO, FFS_LEN(HELLO));
	x(!memcmp(mapd, FFSTR(HELLO)));

	x(0 == ffmmap_unmap(mapd, mapsz));
	ffmmap_close(fmap);

	return 0;
}

int test_filemap()
{
	FFTEST_FUNC;

	char fn[256];
	strcpy(fn, TMP_PATH);
	strcat(fn, "/tmpfile");

	fffile_remove(fn);
	test_mapwr(fn);
	test_mapro(fn);
	test_mapanon();

	fffile_remove(fn);
	return 0;
}

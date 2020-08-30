/** ffos: pipe.h tester
2020, Simon Zolin
*/

#include <FFOS/file.h>
#include <FFOS/string.h>
#include <FFOS/test.h>

#define HELLO "hello\n"
#define FOOBAR "foobar\n"

void test_pipe()
{
	char buf[64];
	fffd rd = FF_BADFD
		, wr = FF_BADFD;

	FFTEST_FUNC;

	x(0 == ffpipe_create2(&rd, &wr, FFO_NONBLOCK));
	x(-1 == ffpipe_read(rd, buf, FFCNT(buf)) && fferr_again(fferr_last()));
	x(0 == ffpipe_nblock(rd, 0));

	x(FFS_LEN(HELLO) == fffile_write(wr, FFSTR(HELLO)));
	x(FFS_LEN(HELLO) == ffpipe_read(rd, buf, FFCNT(buf)));
	x(!memcmp(buf, FFSTR(HELLO)));

	x(0 == ffpipe_close(rd));
	x(0 == ffpipe_close(wr));
}

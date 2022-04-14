/** ffos: pipe.h tester
2020, Simon Zolin
*/

#include <FFOS/pipe.h>
#include <FFOS/file.h>
#include <FFOS/string.h>
#include <FFOS/test.h>

void test_pipe_unnamed()
{
	char buf[64];
	fffd rd, wr;
	x(0 == ffpipe_create2(&rd, &wr, FFPIPE_NONBLOCK));

	x(-1 == ffpipe_read(rd, buf, sizeof(buf)));
	x_sys(fferr_again(fferr_last()));
	x(0 == ffpipe_nonblock(rd, 0));

	x_sys(5 == ffpipe_write(wr, "hello", 5));
	x_sys(5 == ffpipe_read(rd, buf, sizeof(buf)));
	x(!ffmem_cmp(buf, "hello", 5));

	ffpipe_close(rd);
	ffpipe_close(wr);
}

void test_pipe_named()
{
	char buf[64];
	fffd l, c, lc;
#ifdef FF_WIN
	const char *name = FFPIPE_NAME_PREFIX "ffostest.pipe";

	x_sys(FFPIPE_NULL != (l = ffpipe_create_named(name, 0)));

#else

	const char *name = "/tmp/ffostest.unix";
	fffile_remove(name);

	x_sys(FFPIPE_NULL != (l = ffpipe_create_named(name, FFPIPE_NONBLOCK)));

	x(FFPIPE_NULL == (lc = ffpipe_accept(l)));
	x_sys(fferr_again(fferr_last()));
#endif

	x_sys(FFPIPE_NULL != (c = ffpipe_connect(name)));
	x_sys(FFPIPE_NULL != (lc = ffpipe_accept(l)));

	x_sys(5 == ffpipe_write(c, "hello", 5));
	x_sys(5 == ffpipe_read(lc, buf, sizeof(buf)));
	x(!ffmem_cmp(buf, "hello", 5));

	x_sys(6 == ffpipe_write(lc, "foobar", 6));
	x_sys(6 == ffpipe_read(c, buf, sizeof(buf)));
	x(!ffmem_cmp(buf, "foobar", 6));

	ffpipe_peer_close(lc);
	ffpipe_close(l);
	ffpipe_close(c);
#ifdef FF_UNIX
	fffile_remove(name);
#endif
}

void test_pipe()
{
	test_pipe_unnamed();
	test_pipe_named();
}

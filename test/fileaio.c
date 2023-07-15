/** ffos: asyncio.h tester
2020, Simon Zolin
*/

#include <FFOS/asyncio.h>
#include <FFOS/string.h>
#include <FFOS/file.h>
#include <FFOS/dir.h>
#include <ffbase/stringz.h>
#include <FFOS/test.h>

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

	x((ffsize)r == faio.len);
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

	x((ffsize)r == faio.len - faio.bsize - 1);
	x(faio.buf[0] == '!' && faio.buf[r - 1] == '!');
	faio.ok++;
}

void test_fileaio()
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
	x(FFFILE_NULL != (f = fffile_open(fn, O_CREAT | O_RDWR | flags)));
	x(FFFILE_NULL != (faio.kq = ffkqu_create()));
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
	x(FFFILE_NULL != (f2 = fffile_open(fn, O_RDWR)));
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
}

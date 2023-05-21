/** ffos: kcall.h tester
2022, Simon Zolin
*/

#include <FFOS/queue.h>
#include <FFOS/kcall.h>
#include <FFOS/test.h>

static void kc_complete(void *param)
{
	(void)param;
}

void test_kcall()
{
	const char *fn = "kcall.ffos";
	fffile_remove(fn);

	struct ffkcallqueue q = {};
	q.sq = ffrq_alloc(10);
	q.cq = ffrq_alloc(10);
	q.kqpost = FFKQ_NULL;
	struct ffkcall c = {
		.q = &q,
		.handler = kc_complete,
	};

	fffd f = fffile_open_async(fn, FFFILE_CREATE | FFFILE_READWRITE, &c);
	x_sys(f == FFFILE_NULL && fferr_last() == FFKCALL_EINPROGRESS);
	ffkcallq_process_sq(q.sq);
	ffkcallq_process_cq(q.cq);
	f = fffile_open_async(NULL, 0, &c);
	x_sys(f != FFFILE_NULL);

	int r = fffile_write_async(f, "hello", 5, &c);
	x_sys(r < 0 && fferr_last() == FFKCALL_EINPROGRESS);
	ffkcallq_process_sq(q.sq);
	ffkcallq_process_cq(q.cq);
	r = fffile_write_async(FFFILE_NULL, NULL, 0, &c);
	x(r > 0);

	fffileinfo fi;
	r = fffile_info_async(f, &fi, &c);
	x_sys(r != 0 && fferr_last() == FFKCALL_EINPROGRESS);
	ffkcallq_process_sq(q.sq);
	ffkcallq_process_cq(q.cq);
	r = fffile_info_async(FFFILE_NULL, NULL, &c);
	x(r == 0);
	x(fffileinfo_size(&fi) == 5);

	fffile_seek(f, 0, FFFILE_SEEK_BEGIN);

	char buf[10];
	r = fffile_read_async(f, buf, 10, &c);
	x_sys(r < 0 && fferr_last() == FFKCALL_EINPROGRESS);
	ffkcallq_process_sq(q.sq);
	ffkcallq_process_cq(q.cq);
	r = fffile_read_async(FFFILE_NULL, NULL, 0, &c);
	ffstr d = FFSTR_INITN(buf, r);
	xstr(d, "hello");

	fffile_close(f);
	fffile_remove(fn);
}

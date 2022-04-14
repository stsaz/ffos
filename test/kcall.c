/** ffos: kcall.h tester
2022, Simon Zolin
*/

#include <FFOS/queue.h>
#ifdef FF_UNIX
#include <FFOS/kcall.h>
#endif
#include <FFOS/test.h>

void test_kcall()
{
#ifdef FF_UNIX
	struct ffkcallqueue q = {};
	q.sq = ffrq_alloc(10);
	q.cq = ffrq_alloc(10);
	q.kqpost = FFKQ_NULL;
	struct ffkcall c = {
		.q = &q,
	};

	fffd f = fffile_open_async("/etc/fstab", FFFILE_READONLY, &c);
	x(f == FFFILE_NULL && errno == EINPROGRESS);
	ffkcallq_process_sq(q.sq);
	ffkcallq_process_cq(q.cq);
	f = fffile_open_async("/etc/fstab", FFFILE_READONLY, &c);
	x(f != FFFILE_NULL);

	char buf[1000];
	int r = fffile_read_async(f, buf, 1000, &c);
	x(r < 0 && errno == EINPROGRESS);
	ffkcallq_process_sq(q.sq);
	ffkcallq_process_cq(q.cq);
	r = fffile_read_async(f, buf, 1000, &c);
	x(r > 0);

	fffile_close(f);
#endif
}

/** ffos: std.h tester
2020, Simon Zolin
*/

#include <FFOS/std.h>
#include <FFOS/file.h>
#include <FFOS/queue.h>
#include <FFOS/test.h>

#ifdef FF_WIN
#define S1  "Привет\r\n"
#else
#define S1  "Привет\n"
#endif

void test_std()
{
	xint_sys(FFS_LEN(S1), ffstdout_write(S1, FFS_LEN(S1)));
	xint_sys(FFS_LEN(S1), ffstderr_write(S1, FFS_LEN(S1)));

#if 1
	fflog("please enter 'Привет' and press <Enter>");
	char buf[100];
	xint_sys(FFS_LEN(S1), ffstdin_read(buf, sizeof(buf)));
	x(!ffmem_cmp(buf, S1, FFS_LEN(S1)));
#endif
}

void test_std_event()
{
	ffuint attr = FFSTD_LINEINPUT | FFSTD_ECHO;
	x_sys(0 == ffstd_attr(ffstdin, attr, 0));

	ffkq kq = ffkq_create();
	x_sys(kq != FFKQ_NULL);
#ifdef FF_UNIX
	fffile_nonblock(ffstdin, 1);
	x_sys(0 == ffkq_attach(kq, ffstdin, NULL, FFKQ_READ));
#endif

	fflog("Press Ctrl+Right");
	ffstd_ev ev = {};
	int r;
	ffuint key;
	ffstr data = {};

	for (;;) {
		r = ffstd_keyread(ffstdin, &ev, &data);
		fflog("ffstd_event: %d", r);

		if (r == 0) {
			fflog("waiting for key input...");

#ifdef FF_WIN
			r = WaitForSingleObject(ffstdin, 10000);
			xint_sys(WAIT_OBJECT_0, r);
			ffkq_postevent post = ffkq_post_attach(kq, NULL);
			x_sys(0 == ffkq_post(post, NULL));
#endif

			ffkq_event ev;
			ffkq_time t;
			ffkq_time_set(&t, 10000);
			r = ffkq_wait(kq, &ev, 1, t);
			xint_sys(1, r);
			continue;
		}
		x_sys(r > 0);

		key = ffstd_keyparse(&data);
		if (key == (ffuint)(FFKEY_CTRL | FFKEY_RIGHT))
			break;
	}
	xint_sys((ffuint)(FFKEY_CTRL | FFKEY_RIGHT), key);

	x_sys(0 == ffstd_attr(ffstdin, attr, attr));
	ffkq_close(kq);
}

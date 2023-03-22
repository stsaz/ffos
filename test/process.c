/** ffos: process.h tester
2020, Simon Zolin
*/

#include <FFOS/process.h>
#include <FFOS/pipe.h>
#include <FFOS/file.h>
#include <FFOS/queue.h>
#include <FFOS/signal.h>
#include <FFOS/thread.h>
#include <FFOS/test.h>

extern const char *ffostest_argv0;

void test_ps()
{
	ffps h;
	int cod;
	const char *args[6];
	const char *env[4];

	char fn[256];
	const char *rfn = ffps_filename(fn, sizeof(fn), ffostest_argv0);
	x_sys(rfn != NULL);
	fflog("ffps_filename(): '%s'", rfn);

	x(ffps_curid() == ffps_id(ffps_curhdl()));
	if (0) {
		ffps_exit(1);
		(void)ffps_kill(ffps_curhdl());
	}

	ffuint i = 0;
#ifdef FF_UNIX
	args[i++] = "/bin/echo";

#else
	args[i++] = "c:\\windows\\system32\\cmd.exe";
	args[i++] = "/c";
	args[i++] = "echo";
#endif
	args[i++] = "very good";
	args[i++] = "day";
	args[i++] = NULL;
	env[0] = NULL;

	fffd pr, pw;
	x_sys(0 == ffpipe_create(&pr, &pw));
	char buf[32];
	ffps_execinfo info = {};
	info.argv = args;
	info.env = env;
	info.in = FFFILE_NULL;
	info.out = pw;
	info.err = pw;
	h = ffps_exec_info(args[0], &info);
	x_sys(h != FFPS_NULL);
	fflog("\ncreated process: %d", (int)ffps_id(h));
	x_sys(0 == ffps_wait(h, -1, &cod));
	fflog("process exited with code %d", (int)cod);
#ifdef FF_UNIX
	x_sys(14 == ffpipe_read(pr, buf, sizeof(buf)));
	buf[14] = '\0';
	x(!strcmp(buf, "very good day\n"));
#else
	x_sys(17 == ffpipe_read(pr, buf, sizeof(buf)));
	buf[17] = '\0';
	x(!strcmp(buf, "\"very good\" day\r\n"));
#endif
	ffpipe_close(pr);
	ffpipe_close(pw);


#ifdef FF_UNIX
	args[0] = "/bin/sh";
	args[1] = NULL;
#else
	args[0] = "c:\\windows\\system32\\cmd.exe";
	args[1] = NULL;
#endif

	x_sys(FFPS_NULL != (h = ffps_exec(args[0], args, env)));
	x_sys(0 != ffps_wait(h, 0, &cod) && fferr_last() == FFERR_TIMEOUT);
	(void)ffps_kill(h);
	x_sys(0 == ffps_wait(h, -1, &cod));
	fflog("process exited with code %d", (int)cod);
}

/*
. create pipe
. create kqueue; attach SIGCHLD, pipe-read-fd
. create process
. wait for data from pipe and the process to finish */
void test_ps_kq()
{
#ifdef FF_LINUX
	ffkq kq = ffkq_create();

	int sigs[] = { SIGCHLD };
	ffkqsig kqsig = ffkqsig_attach(kq, sigs, 1, (void*)1);
	x_sys(kqsig != FFKQSIG_NULL);

	ffps_execinfo info = {};
	const char *args[3];
	ffuint i = 0;
	args[i++] = "/bin/echo";
	args[i++] = "HI";
	args[i++] = NULL;
	info.argv = args;

	fffd pr, pw;
	x_sys(0 == ffpipe_create(&pr, &pw));
	x_sys(0 == ffkq_attach(kq, pr, (void*)2, FFKQ_READ));
	x_sys(0 == ffpipe_nonblock(pr, 1));
	info.in = FFFILE_NULL;
	info.out = pw;
	info.err = pw;

	char buf[32];
	int r = ffpipe_read(pr, buf, sizeof(buf));
	xint_sys(r, -1);
	x(fferr_again(fferr_last()));

	ffps ps = ffps_exec_info(args[0], &info);
	x_sys(ps != FFPS_NULL);
	fflog("\ncreated process: %d", (int)ffps_id(ps));

	// ffthd_sleep(1000);

	int flags = 0;
	while (flags != 3) {
		ffkq_event ev;
		ffkq_time t;
		ffkq_time_set(&t, -1);
		fflog("ffkq_wait...");
		r = ffkq_wait(kq, &ev, 1, t);
		fflog("ffkq_wait: %d", r);
		xint_sys(r, 1);

		if (ffkq_event_data(&ev) == (void*)2) {
			r = ffpipe_read(pr, buf, sizeof(buf));
			xint_sys(r, 3);
			ffstr s = FFSTR_INITN(buf, r);
			xseq(&s, "HI\n");
			flags |= 1;

			r = ffpipe_read(pr, buf, sizeof(buf));
			xint_sys(r, -1);
			x(fferr_again(fferr_last()));

		} else if (ffkq_event_data(&ev) == (void*)1) {
			int sig = ffkqsig_read(kqsig, &ev);
			xint_sys(sig, SIGCHLD);

			int code;
			x_sys(0 == ffps_wait(ps, -1, &code));
			ps = FFPS_NULL;
			fflog("process exited: %d", code);
			flags |= 2;

		} else {
			x(0);
		}
	}

	ffkqsig_detach(kqsig, kq);
	ffkq_close(kq);
	ffps_close(ps);
	ffpipe_close(pr);
	ffpipe_close(pw);
#endif
}

#ifdef FF_LINUX
/** Exec, wait and get all output data */
static inline int ffps_exec_info_wait(const char *filename, ffps_execinfo *info, int *exit_code, ffvec *output, ffvec *error, ffuint flags)
{
	(void)flags;
	ffps ps = FFPS_NULL;
	int rc = -1, r;
	fffd rd = FFPIPE_NULL, wr = FFPIPE_NULL;
	fffd rd_err = FFPIPE_NULL, wr_err = FFPIPE_NULL;
	enum { I_KQ, I_SIG, I_OUT, I_ERR, I_BOTH, };

	info->in = FFFILE_NULL;
	info->out = FFFILE_NULL;
	info->err = FFFILE_NULL;

	ffkqsig kqsig = FFKQSIG_NULL;
	ffkq kq = ffkq_create();
	if (kq == FFKQ_NULL)
		goto end;
	ffkq_event ev;
	ffkq_time t;
	ffkq_time_set(&t, -1);

	int sigs[] = { SIGCHLD };
	kqsig = ffkqsig_attach(kq, sigs, 1, (void*)I_SIG);
	if (kqsig == FFKQSIG_NULL)
		goto end;

	if (output != NULL) {
		if (0 != ffpipe_create2(&rd, &wr, FFPIPE_NONBLOCK))
			goto end;
		if (0 != ffkq_attach(kq, rd, (void*)I_OUT, FFKQ_READ))
			goto end;
		info->out = wr;
	}

	if (error != NULL) {
		if (0 != ffpipe_create2(&rd_err, &wr_err, FFPIPE_NONBLOCK))
			goto end;
		if (0 != ffkq_attach(kq, rd_err, (void*)I_ERR, FFKQ_READ))
			goto end;
		info->err = wr_err;
	}

	ps = ffps_exec_info(filename, info);
	if (ps == FFPS_NULL)
		goto end;

	int status = 0, state = I_KQ;
	while (status != (1|2|4)) {
		switch (state) {

		case I_KQ:
			r = ffkq_wait(kq, &ev, 1, t);
			if (r < 0 && fferr_last() == EINTR)
				continue;
			else if (r != 1)
				goto end;
			state = (ffsize)ffkq_event_data(&ev);
			break;

		case I_SIG:
			if (0 == ffps_wait(ps, -1, exit_code)) {
				ps = FFPS_NULL;
				status |= 1;
				state = I_BOTH;
				if (output == NULL && error == NULL)
					status |= 2|4;
			} else {
				state = I_KQ;
			}
			break;

		case I_OUT:
		case I_ERR:
		case I_BOTH: {
			ffvec *v = output;
			fffd p = rd;
			int bit = 2;
			if (state == I_ERR) {
				v = error;
				p = rd_err;
				bit = 4;
			}

			for (;;) {
				ffvec_growT(v, 4096, char);
				r = ffpipe_read(p, ffslice_endT(v, char), ffvec_unused(v));
				if (r < 0 && fferr_again(fferr_last())) {
					status |= bit;
					break;
				} else if (r <= 0) {
					goto end;
				}
				v->len += r;
			}

			state = (state == I_BOTH) ? I_ERR : I_KQ;
			break;
		}
		}
	}

	rc = 0;

end:
	ffpipe_close(rd);
	ffpipe_close(wr);
	ffpipe_close(rd_err);
	ffpipe_close(wr_err);
	ffps_close(ps);
	ffkqsig_detach(kqsig, kq);
	ffkq_close(kq);
	return rc;
}
#endif

void test_ps_out()
{
#ifdef FF_LINUX
	ffps_execinfo info = {};
	const char *args[3];
	args[0] = "echo";
	args[1] = "hello";
	args[2] = NULL;
	info.argv = args;
	ffvec out = {}, err = {};
	x(0 == ffps_exec_info_wait("/bin/echo", &info, NULL, &out, &err, 0));
	xseq((ffstr*)&out, "hello\n");
	ffvec_free(&out);
	ffvec_free(&err);
#endif
}

void test_process()
{
#ifdef FF_LINUX
	int sigs[] = { SIGCHLD };
	ffsig_mask(SIG_BLOCK, sigs, 1);
#endif

	test_ps();
	test_ps_kq();
	test_ps_out();
}

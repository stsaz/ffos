/** ffos: process.h tester
2020, Simon Zolin
*/

#include <FFOS/process.h>
#include <FFOS/pipe.h>
#include <FFOS/file.h>
#include <FFOS/test.h>

extern const char *ffostest_argv0;

void test_process()
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

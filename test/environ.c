/** ffos: environ.h tester
2020, Simon Zolin
*/

#include <FFOS/environ.h>
#include <FFOS/test.h>

#if defined FF_BSD || defined FF_APPLE
extern char **environ;
#endif

void test_env()
{
	char *p, buf[256];
	ffstr s;

#ifdef FF_UNIX
	ffenv_init(NULL, environ);
	char **ee = _ff_environ;
	char *e_s[] = { "KEY1=VAL1", "KEY=VAL", NULL };
	_ff_environ = e_s;

	p = ffenv_expand(NULL, NULL, 0, "asdf $KEY$KEY2 1234");
	ffstr_setz(&s, p);
	xseq(&s, "asdf VAL 1234");
	ffmem_free(p);

	p = ffenv_expand(NULL, buf, sizeof(buf), "asdf $KEY$KEY1 1234");
	x(p == buf);
	ffstr_setz(&s, p);
	xseq(&s, "asdf VALVAL1 1234");

	_ff_environ = ee;

#else
	p = ffenv_expand(NULL, buf, sizeof(buf), "asdf %WINDIR%%WINDIR1% 1234");
	x(p == buf);
	ffstr_setz(&s, p);
	x(ffstr_ieqz(&s, "asdf C:\\WINDOWS%WINDIR1% 1234"));

	p = ffenv_expand(NULL, NULL, 0, "asdf %WINDIR%%WINDIR1% 1234");
	ffstr_setz(&s, p);
	x(ffstr_ieqz(&s, "asdf C:\\WINDOWS%WINDIR1% 1234"));
	ffmem_free(p);
#endif

	(void) ffenv_locale(FFENV_LANGUAGE);
}

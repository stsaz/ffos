/** ffos: signal.h tester
2020, Simon Zolin
*/

#include <FFOS/sig.h>
#include <FFOS/thread.h>
#include <FFOS/test.h>

#ifdef FF_UNIX
#include <FFOS/signal.h>
void test_unixsignal()
{
	ffkq kq = ffkq_create();
	ffkq_event ev;
	ffkq_time tm;
	ffkq_time_set(&tm, 1000);
#ifdef FF_APPLE
	ffkq_time_set(&tm, 15000);
#endif

	const int sigs[] = { SIGINT, SIGHUP, SIGUSR1, SIGUSR2 };
	ffsig_mask(SIG_BLOCK, sigs, FF_COUNT(sigs));

	ffkqsig sig = ffkqsig_attach(kq, sigs, FF_COUNT(sigs), (void*)0x12345678);
	x_sys(sig != FFKQSIG_NULL);

	raise(sigs[0]);
	raise(sigs[1]);
#ifdef FF_APPLE
	// raise() doesn't trigger events
	fflog("Please execute: kill -INT %d ; kill -HUP %d; kill -USR1 %d ; kill -USR2 %d"
		, getpid(), getpid(), getpid(), getpid());
#endif

	// wait for and process signal #1
	fflog("waiting for signal...");
	xint_sys(1, ffkq_wait(kq, &ev, 1, tm));
	xieq((ffsize)0x12345678, (ffsize)ffkq_event_data(&ev));
	int signo = ffkqsig_read(sig, &ev);
	x_sys(signo == sigs[0] || signo == sigs[1]);

#if defined FF_BSD || defined FF_APPLE
	// wait for signal #2
	xint_sys(-1, ffkqsig_read(sig, &ev));
	fflog("waiting for signal...");
	xint_sys(1, ffkq_wait(kq, &ev, 1, tm));
	xieq((ffsize)0x12345678, (ffsize)ffkq_event_data(&ev));
#endif

	// process signal #2
	int signo2 = ffkqsig_read(sig, &ev);
	if (signo == sigs[0])
		xieq(sigs[1], signo2);
	else
		xieq(sigs[0], signo2);

	xint_sys(-1, ffkqsig_read(sig, &ev));

	for (int i = 2;  i != FF_COUNT(sigs);  i++) {
		raise(sigs[i]);

		fflog("waiting for signal...");
		xint_sys(1, ffkq_wait(kq, &ev, 1, tm));
		xieq((ffsize)0x12345678, (ffsize)ffkq_event_data(&ev));

		signo = ffkqsig_read(sig, &ev);
		xieq(sigs[i], signo);

		xint_sys(-1, ffkqsig_read(sig, &ev));
	}

	ffkqsig_detach(sig, kq);
	ffkq_close(kq);
}
#endif

static void sig_handler(struct ffsig_info *inf)
{
	fflog("Signal:%xu  Address:0x%p  Flags:%xu"
		, inf->sig, inf->addr, inf->flags);
}

int FFTHREAD_PROCCALL sig_thdfunc(void *param)
{
	ffuint sig = (ffsize)param;
	if (sig == FFSIG_STACK) {
		const ffuint sigs_fault[] = { FFSIG_STACK };
		ffsig_subscribe(&sig_handler, sigs_fault, FF_COUNT(sigs_fault));
	}
	ffsig_raise(sig);
	return 0;
}

void test_sig(int sig)
{
	static const ffuint sigs_fault[] = { FFSIG_ABORT, FFSIG_SEGV, FFSIG_STACK, FFSIG_ILL, FFSIG_FPE };
	ffsig_subscribe(&sig_handler, sigs_fault, FF_COUNT(sigs_fault));
	ffthread th = ffthread_create(sig_thdfunc, (void*)(ffsize)sig, 0);
	ffthread_join(th, -1, NULL);
}

void test_sig_abort()
{
	test_sig(FFSIG_ABORT);
}
void test_sig_segv()
{
	test_sig(FFSIG_SEGV);
}
void test_sig_stack()
{
	test_sig(FFSIG_STACK);
}
void test_sig_fpe()
{
	test_sig(FFSIG_FPE);
}

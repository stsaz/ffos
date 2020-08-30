/** ffos: signal.h tester
2020, Simon Zolin
*/

#include <FFOS/signal.h>
#include <test/test.h>

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

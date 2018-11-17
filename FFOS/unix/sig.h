/**
Copyright (c) 2016 Simon Zolin
*/

#pragma once


enum FFSIG {
	FFSIG_SEGV = SIGSEGV, //Invalid memory reference
	FFSIG_ABORT = SIGABRT, //Abort signal
	FFSIG_FPE = SIGFPE, //Floating-point exception
	FFSIG_ILL = SIGILL, //Illegal Instruction
	FFSIG_STACK = 0x80000000 | SIGSEGV, //Stack overflow
};

enum FFSIGINFO_F {
	_FFSIGINFO_FDUMMY,

	//SIGSEGV:
	// SEGV_MAPERR //Address not mapped to object
	// SEGV_ACCERR //Invalid permissions for mapped object
};

/** Modify current signal mask. */
static FFINL int ffsig_mask(int how, const int *sigs, size_t nsigs) {
	sigset_t mask;
	size_t i;
	sigemptyset(&mask);
	for (i = 0;  i < nsigs;  i++) {
		sigaddset(&mask, sigs[i]);
	}
	return sigprocmask(how, &mask, NULL);
}

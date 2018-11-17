/**
Copyright (c) 2016 Simon Zolin
*/

#pragma once


#if defined FF_MSVC || defined FF_MINGW
enum {
	SIG_BLOCK,
	SIG_UNBLOCK,
};

enum {
	SIGINT = 1,
	SIGIO,
	SIGHUP,
	SIGUSR1,
};
#endif

enum FFSIG {
	FFSIG_SEGV = EXCEPTION_ACCESS_VIOLATION, //The thread tried to read from or write to a virtual address for which it does not have the appropriate access
	FFSIG_ABORT = EXCEPTION_BREAKPOINT, //A breakpoint was encountered
	FFSIG_ILL = EXCEPTION_ILLEGAL_INSTRUCTION, //The thread tried to execute an invalid instruction
	FFSIG_STACK = EXCEPTION_STACK_OVERFLOW, //The thread used up its stack
	FFSIG_FPE = EXCEPTION_FLT_DIVIDE_BY_ZERO,
};

enum FFSIGINFO_F {
	_FFSIGINFO_FDUMMY,

	//EXCEPTION_ACCESS_VIOLATION:
	// =0 //the thread attempted to read the inaccessible data
	// =1 //the thread attempted to write to an inaccessible address
	// =8 //the thread causes a user-mode data execution prevention (DEP) violation
};

typedef ffkevent ffsignal;

#define ffsig_init(sig)  ffkev_init(sig)

static FFINL int ffsig_mask(int how, const int *sigs, size_t nsigs)
{
	return 0;
}

typedef int ffsiginfo;

FF_EXTN int ffsig_read(ffsignal *sig, ffsiginfo *si);

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

static FFINL int ffsig_mask(int how, const int *sigs, size_t nsigs)
{
	return 0;
}

typedef int ffsiginfo;

FF_EXTN int ffsig_read(ffsignal *sig, ffsiginfo *si);

/**
Copyright (c) 2016 Simon Zolin
*/

#pragma once


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

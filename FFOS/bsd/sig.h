/**
Copyright (c) 2016 Simon Zolin
*/

#pragma once


typedef ffaio_task ffsignal;

#define ffsig_init(sig)  ffaio_init(sig)

typedef int ffsiginfo;

static FFINL int ffsig_read(ffsignal *t, ffsiginfo *si)
{
	int ident = t->ev->ident;
	if (t->ev->ident == (size_t)-1) {
		errno = EAGAIN;
		return -1;
	}
	t->ev->ident = -1;
	return ident;
}

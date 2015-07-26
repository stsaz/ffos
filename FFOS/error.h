/**
System error.
Copyright (c) 2013 Simon Zolin
*/

#pragma once

#include <FFOS/types.h>

#ifdef FF_UNIX
#include <FFOS/unix/error.h>
#elif defined FF_WIN
#include <FFOS/win/error.h>
#endif

enum FFERR {
	FFERR_OK = 0
	, FFERR_INTERNAL

//general
	, FFERR_READ
	, FFERR_WRITE

//memory
	, FFERR_BUFALOC
	, FFERR_BUFGROW

//file
	, FFERR_FOPEN
	, FFERR_FSEEK
	, FFERR_FMAP
	, FFERR_FDEL
	, FFERR_FCLOSE
	, FFERR_FRENAME

	, FFERR_DIROPEN

//sys
	, FFERR_TMRINIT
	, FFERR_KQUCREAT
	, FFERR_KQUATT
	, FFERR_KQUWAIT
	, FFERR_THDCREAT
	, FFERR_NBLOCK
	, FFERR_PSFORK

	, FFERR_DLOPEN
	, FFERR_DLADDR

//socket
	, FFERR_SKTCREAT
	, FFERR_SKTBIND
	, FFERR_SKTOPT
	, FFERR_SKTSHUT
	, FFERR_SKTLISTEN
	, FFERR_SKTCONN
	, FFERR_RESOLVE

	, FFERR_SYSTEM
};

FF_EXTN const char * fferr_opstr(enum FFERR e);

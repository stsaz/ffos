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
	, FFERR_DIRMAKE

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

#define ffmem_alloc_S  "memory alloc"
#define ffmem_realloc_S  "memory realloc"

#define fffile_open_S  "file open"
#define fffile_seek_S  "file seek"
#define fffile_map_S  "file map"
#define fffile_rm_S  "file remove"
#define fffile_close_S  "file close"
#define fffile_rename_S  "file rename"
#define fffile_info_S  "get file info"
#define fffile_settime_S  "set file time"
#define fffile_read_S  "read"
#define fffile_write_S  "write"
#define fffile_nblock_S  "set non-blocking mode"

#define ffdir_open_S  "directory open"
#define ffdir_make_S  "directory make"
#define ffdir_rm_S  "directory remove"

#define ffpipe_create_S  "pipe create"
#define ffpipe_open_S  "pipe open"

#define ffkqu_create_S  "kqueue create"
#define ffkqu_attach_S  "kqueue attach"
#define ffkqu_wait_S  "kqueue wait"

#define ffskt_create_S  "socket create"
#define ffskt_bind_S  "socket bind"
#define ffskt_setopt_S  "socket option"
#define ffskt_nblock_S  "set non-blocking mode"
#define ffskt_shut_S  "socket shutdown"
#define ffskt_connect_S  "socket connect"
#define ffskt_listen_S  "socket listen"
#define ffskt_send_S  "socket send"
#define ffskt_recv_S  "socket recv"

#define ffdl_open_S  "library open"
#define ffdl_addr_S  "get library symbol"

#define fftmr_create_S  "timer create"
#define fftmr_start_S  "timer start"
#define ffthd_create_S  "thread create"
#define ffps_fork_S  "process fork"
#define ffaddr_info_S  "address resolve"

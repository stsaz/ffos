/** Signals.
Copyright (c) 2013 Simon Zolin
*/

#pragma once

#include <FFOS/asyncio.h>
#if defined FF_LINUX
	#include <FFOS/linux/sig.h>
#elif defined FF_BSD || defined FF_APPLE
	#include <FFOS/bsd/sig.h>
#endif
#if defined FF_UNIX
	#include <FFOS/unix/sig.h>
#elif defined FF_WIN
	#include <FFOS/win/sig.h>
#endif


/** If 'handler' is set, initialize kernel event for signals.
If 'handler' is NULL, remove event from the kernel.
Windows: Ctrl-events handling sequence: ctrl_handler() -> pipe -> kqueue -> sig_handler()
*/
FF_EXTN int ffsig_ctl(ffsignal *sig, fffd kq, const int *sigs, size_t nsigs, ffaio_handler handler);


struct ffsig_info {
	uint sig; //enum FFSIG
	void *addr; //FFSIG_SEGV: the virtual address of the inaccessible data
	uint flags; //enum FFSIGINFO_F
	void *si; //OS-specific information object
};

typedef void (*ffsig_handler)(struct ffsig_info *i);

/** Subscribe to system signals.
handler: Set signal handler.  Only one handler per process can be used.
 NULL: use the current handler
Return 0 on success */
FF_EXTN int ffsig_subscribe(ffsig_handler handler, const uint *sigs, uint nsigs);

/** Raise the specified signal. */
FF_EXTN void ffsig_raise(uint sig);

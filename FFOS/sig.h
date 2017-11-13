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

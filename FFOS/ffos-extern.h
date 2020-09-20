/** ffos: define external symbols
*/

#include <FFOS/base.h>

#ifdef FF_WIN

#include <FFOS/dylib.h>
signed char _ffdl_open_supports_flags;

#include <FFOS/error.h>
char _fferr_buffer[1024];

#include <FFOS/socket.h>
LPFN_DISCONNECTEX _ff_DisconnectEx;
LPFN_CONNECTEX _ff_ConnectEx;
LPFN_ACCEPTEX _ff_AcceptEx;
LPFN_GETACCEPTEXSOCKADDRS _ff_GetAcceptExSockaddrs;

#include <FFOS/perf.h>
LARGE_INTEGER _fftime_perffreq;
_ff_NtQuerySystemInformation_t _ff_NtQuerySystemInformation;
_ff_GetProcessMemoryInfo_t _ff_GetProcessMemoryInfo;

#else

#include <FFOS/environ.h>
char **_ff_environ;

#endif

#include <FFOS/signal.h>
ffsig_handler _ffsig_userhandler;

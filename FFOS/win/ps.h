/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/file.h>


static inline int ffenv_update(void)
{
	DWORD_PTR r;
	return !SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"Environment", SMTO_ABORTIFHUNG, 5000, &r);
}


typedef HANDLE ffps;
#define FFPS_INV  INVALID_HANDLE_VALUE

/** Start a new process with command line.
Return FFPS_INV on error. */
FF_EXTN ffps ffps_exec_cmdln(const char *filename, const char *cmdln);
FF_EXTN ffps ffps_exec_cmdln_q(const ffsyschar *filename, ffsyschar *cmdln);

#define ffps_id  GetProcessId

/** Send signal to a process.  Works only if the other process has called ffsig_ctl(). */
FF_EXTN int ffps_sig(int pid, int sig);

#define ffps_kill(h)  (0 == TerminateProcess(h, -9 /*-SIGKILL*/))

static FFINL int ffps_close(ffps h) {
	return 0 == CloseHandle(h);
}

#define ffps_curhdl  GetCurrentProcess

#define ffps_curid  GetCurrentProcessId

#define ffps_exit  ExitProcess

/** Reset or disable a system timer.
flags:
 ES_SYSTEM_REQUIRED | ES_AWAYMODE_REQUIRED: don't put the system to sleep
 ES_DISPLAY_REQUIRED: don't switch off display
 ES_CONTINUOUS:
  0: reset once
  1 + flags: disable
  1 + no flags: restore default behaviour
*/
#define ffps_systimer(flags)  SetThreadExecutionState(flags)


enum FFSC_I {
	_SC_PAGESIZE = 1
	, _SC_NPROCESSORS_ONLN
};

typedef SYSTEM_INFO ffsysconf;

static FFINL void ffsc_init(ffsysconf *sc) {
	GetNativeSystemInfo(sc);
}

static FFINL int ffsc_get(ffsysconf *sc, int name) {
	switch (name) {
	case _SC_PAGESIZE:
		return sc->dwAllocationGranularity;
	case _SC_NPROCESSORS_ONLN:
		return sc->dwNumberOfProcessors;
	}
	return 0;
}


enum FFLANG_F {
	FFLANG_FLANG = LOCALE_ILANGUAGE, // enum FFLANG
};

enum FFLANG {
	FFLANG_NONE,
	FFLANG_ENG = LANG_ENGLISH,
	FFLANG_ESP = LANG_SPANISH,
	FFLANG_FRA = LANG_FRENCH,
	FFLANG_GER = LANG_GERMAN,
	FFLANG_RUS = LANG_RUSSIAN,
};

/** Get user locale information.
flags: enum FFLANG_F
*/
static inline uint fflang_info(uint flags)
{
	WCHAR val[4];
	GetLocaleInfo(LOCALE_USER_DEFAULT, flags | LOCALE_RETURN_NUMBER, val, FFCNT(val));
	return *(ushort*)val & 0xff;
}

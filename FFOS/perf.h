/** ffos: performance counters
2020, Simon Zolin
*/

/*
ffps_perf
ffthread_perf
ffps_perf_diff ffps_perf_add
*/

#pragma once

#include <FFOS/string.h>
#include <FFOS/time.h>

enum FFPS_PERF {
	FFPS_PERF_REALTIME = 1,
	FFPS_PERF_CPUTIME = 2, // summarized CPU time
	FFPS_PERF_SEPTIME = 4, // user and system CPU time
	FFPS_PERF_RUSAGE = FFPS_PERF_SEPTIME | 8, // user/systime, pagefaults, maxrss, in/outblock, v/ivctxsw
};

struct ffps_perf {
	fftime realtime;
	fftime cputime; // UNIX: higher precision than usertime/systime
	fftime usertime;
	fftime systime;
	ffuint pagefaults;
	ffuint maxrss; // in Kb
	ffuint inblock;
	ffuint outblock;
	ffuint vctxsw;
	ffuint ivctxsw;
};

#ifdef FF_WIN

#include <psapi.h>
#include <winternl.h>
#include <ntstatus.h>

typedef NTSTATUS WINAPI (*_ff_NtQuerySystemInformation_t)(SYSTEM_INFORMATION_CLASS, PVOID, ULONG, PULONG);
FF_EXTN _ff_NtQuerySystemInformation_t _ff_NtQuerySystemInformation;

typedef BOOL WINAPI (*_ff_GetProcessMemoryInfo_t)(HANDLE, PROCESS_MEMORY_COUNTERS*, DWORD);
FF_EXTN _ff_GetProcessMemoryInfo_t _ff_GetProcessMemoryInfo;

FF_EXTN LARGE_INTEGER _fftime_perffreq;

typedef struct _FF_SYSTEM_THREAD_INFORMATION {
	LARGE_INTEGER Reserved1[3];
	ULONG Reserved2;
	PVOID StartAddress;
	CLIENT_ID ClientId;
	KPRIORITY Priority;
	LONG BasePriority;
	ULONG Reserved3; // ContextSwitchCount
	ULONG ThreadState;
	ULONG WaitReason;
} _FF_SYSTEM_THREAD_INFORMATION;

struct _ffpsinfo_s {
	SYSTEM_PROCESS_INFORMATION psinfo;
	_FF_SYSTEM_THREAD_INFORMATION threads[0];
};

/** Get information about all running processes and their threads */
static inline void* _ffps_info_get(void)
{
	void *data;
	const ffuint N_PROC = 50;
	const ffuint N_THD = 10;
	ffuint cap = N_PROC * (sizeof(SYSTEM_PROCESS_INFORMATION) + N_THD * sizeof(_FF_SYSTEM_THREAD_INFORMATION));
	int r;
	ULONG n;

	if (_ff_NtQuerySystemInformation == NULL) {
		HANDLE dl_ntdll; // Note: we don't unload it
		if (NULL == (dl_ntdll = LoadLibraryExW(L"ntdll.dll", NULL, 0)))
			return NULL;
		if (NULL == (_ff_NtQuerySystemInformation = (_ff_NtQuerySystemInformation_t)(void*)GetProcAddress((HMODULE)dl_ntdll, "NtQuerySystemInformation")))
			return NULL;
	}

	for (ffuint i = 0;  i != 2;  i++) {
		if (NULL == (data = ffmem_alloc(cap))) {
			r = -1;
			break;
		}
		r = _ff_NtQuerySystemInformation(SystemProcessInformation, data, cap, &n);
		if (r != STATUS_INFO_LENGTH_MISMATCH)
			break;

		ffmem_free(data);
		data = NULL;
		cap = ff_align_ceil2(n, 4096);
	}

	if (r != 0 || n < sizeof(SYSTEM_PROCESS_INFORMATION)) {
		ffmem_free(data);
		return NULL;
	}

	return data;
}

/** Get the next _ffpsinfo_s object or return NULL if there's no more */
static inline struct _ffpsinfo_s* _ffps_info_next(struct _ffpsinfo_s *pi)
{
	if (pi->psinfo.NextEntryOffset == 0)
		return NULL;
	return (struct _ffpsinfo_s*)((ffbyte*)pi + pi->psinfo.NextEntryOffset);
}

static inline void _fftime_from_filetime(fftime *t, const FILETIME *ft)
{
	ffuint64 n = ((ffuint64)ft->dwHighDateTime << 32) | ft->dwLowDateTime;
	t->sec = n / (1000000 * 10);
	t->nsec = (n % (1000000 * 10)) * 100;
}

static inline fftime _fftime_perf()
{
	if (_fftime_perffreq.QuadPart == 0)
		QueryPerformanceFrequency(&_fftime_perffreq);

	LARGE_INTEGER val;
	QueryPerformanceCounter(&val);
	fftime t;
	t.sec = val.QuadPart / _fftime_perffreq.QuadPart;
	t.nsec = (val.QuadPart % _fftime_perffreq.QuadPart) * 1000000000 / _fftime_perffreq.QuadPart;
	return t;
}

static inline int ffps_perf(struct ffps_perf *p, ffuint flags)
{
	int rc = 0, r;
	HANDLE ps = GetCurrentProcess();

	if (flags & FFPS_PERF_REALTIME) {
		p->realtime = _fftime_perf();
	}

	if (flags & (FFPS_PERF_SEPTIME | FFPS_PERF_CPUTIME)) {
		FILETIME cre, ex, kern, usr;
		if (0 != (r = GetProcessTimes(ps, &cre, &ex, &kern, &usr))) {
			_fftime_from_filetime(&p->systime, &kern);
			_fftime_from_filetime(&p->usertime, &usr);
			if (flags & FFPS_PERF_CPUTIME) {
				p->cputime = p->systime;
				fftime_add(&p->cputime, &p->usertime);
			}
		}
		rc |= !r;
	}

	if (flags & FFPS_PERF_RUSAGE) {
		IO_COUNTERS io;
		if (0 != (r = GetProcessIoCounters(ps, &io))) {
			p->inblock = io.ReadOperationCount;
			p->outblock = io.WriteOperationCount;
		}
		rc |= !r;
	}

	if (flags & FFPS_PERF_RUSAGE) {
		PROCESS_MEMORY_COUNTERS mc;
		if (_ff_GetProcessMemoryInfo == NULL) {
			HANDLE dl_psapi; // Note: we don't unload it
			if (NULL != (dl_psapi = LoadLibraryExW(L"psapi.dll", NULL, 0))) {
				_ff_GetProcessMemoryInfo = (_ff_GetProcessMemoryInfo_t)(void*)GetProcAddress((HMODULE)dl_psapi, "GetProcessMemoryInfo");
			}
		}
		if (_ff_GetProcessMemoryInfo != NULL
			&& 0 != (r = _ff_GetProcessMemoryInfo(ps, &mc, sizeof(mc)))) {

			p->pagefaults = mc.PageFaultCount;
			p->maxrss = mc.PeakWorkingSetSize / 1024;
		}
		rc |= !r;
	}

	if (flags & FFPS_PERF_RUSAGE) {
		void *data = _ffps_info_get();
		if (data == NULL) {
			rc |= 1;
		} else {
			ffuint cur_pid = GetCurrentProcessId();

			for (struct _ffpsinfo_s *pi = (struct _ffpsinfo_s*)data;  pi != NULL;  pi = _ffps_info_next(pi)) {

				if ((ffsize)pi->psinfo.UniqueProcessId == cur_pid) {
					for (ffuint i = 0;  i != pi->psinfo.NumberOfThreads;  i++) {
						const _FF_SYSTEM_THREAD_INFORMATION *ti = &pi->threads[i];
						p->vctxsw += ti->Reserved3;
					}
					break;
				}
			}

			ffmem_free(data);
		}
	}

	p->ivctxsw = 0;
	return rc;
}

static inline int ffthread_perf(struct ffps_perf *p, ffuint flags)
{
	int rc = 0, r;
	HANDLE th = GetCurrentThread();

	if (flags & FFPS_PERF_REALTIME) {
		p->realtime = _fftime_perf();
	}

	if (flags & (FFPS_PERF_SEPTIME | FFPS_PERF_CPUTIME)) {
		FILETIME cre, ex, kern, usr;
		if (0 != (r = GetThreadTimes(th, &cre, &ex, &kern, &usr))) {
			_fftime_from_filetime(&p->systime, &kern);
			_fftime_from_filetime(&p->usertime, &usr);
			if (flags & FFPS_PERF_CPUTIME) {
				p->cputime = p->systime;
				fftime_add(&p->cputime, &p->usertime);
			}
		}
		rc |= !r;
	}

	if (flags & FFPS_PERF_RUSAGE) {
		void *data = _ffps_info_get();
		if (data == NULL) {
			rc |= 1;
		} else {
			ffuint cur_pid = GetCurrentProcessId();
			ffuint cur_tid = GetCurrentThreadId();

			for (struct _ffpsinfo_s *pi = (struct _ffpsinfo_s*)data;  pi != NULL;  pi = _ffps_info_next(pi)) {

				if ((ffsize)pi->psinfo.UniqueProcessId == cur_pid) {
					for (ffuint i = 0;  i != pi->psinfo.NumberOfThreads;  i++) {
						const _FF_SYSTEM_THREAD_INFORMATION *ti = &pi->threads[i];
						if ((ffsize)ti->ClientId.UniqueThread == cur_tid) {
							p->vctxsw += ti->Reserved3;
							break;
						}
					}
					break;
				}
			}

			ffmem_free(data);
		}
	}

	p->maxrss = 0;
	p->inblock = 0;
	p->outblock = 0;
	p->ivctxsw = 0;
	return rc;
}

#else // UNIX:

#include <sys/resource.h>

static inline int _ffps_perf(int who, struct ffps_perf *p, ffuint flags)
{
	int rc = 0, r;
	struct timespec ts;

	if (flags & FFPS_PERF_REALTIME) {
		if (0 == (r = clock_gettime(CLOCK_MONOTONIC, &ts))) {
			fftime_fromtimespec(&p->realtime, &ts);
		}
		rc |= r;
	}

	if (flags & FFPS_PERF_RUSAGE) {
		struct rusage u;
		if (0 == (r = getrusage(who, &u))) {
			fftime_fromtimeval(&p->usertime, &u.ru_utime);
			fftime_fromtimeval(&p->systime, &u.ru_stime);
			p->pagefaults = u.ru_minflt + u.ru_majflt;
			p->maxrss = u.ru_maxrss;
			p->inblock = u.ru_inblock;
			p->outblock = u.ru_oublock;
			p->vctxsw = u.ru_nvcsw;
			p->ivctxsw = u.ru_nivcsw;
		}
		rc |= r;
	}

	if (flags & FFPS_PERF_CPUTIME) {
		clockid_t c = (who == RUSAGE_SELF) ? CLOCK_PROCESS_CPUTIME_ID : CLOCK_THREAD_CPUTIME_ID;
		if (0 == (r = clock_gettime(c, &ts))) {
			fftime_fromtimespec(&p->cputime, &ts);
		}
		rc |= r;
	}

	return rc;
}

static inline int ffps_perf(struct ffps_perf *p, ffuint flags)
{
	return _ffps_perf(RUSAGE_SELF, p, flags);
}

static inline int ffthread_perf(struct ffps_perf *p, ffuint flags)
{
#if defined FF_APPLE
	int r = _ffps_perf(RUSAGE_SELF + 1, p, flags & ~FFPS_PERF_RUSAGE);
	if (r == 0 && (flags & FFPS_PERF_RUSAGE)) {
		r = -1;
		errno = ENOSYS;
	}
	return r;

#else
	return _ffps_perf(RUSAGE_THREAD, p, flags);
#endif
}

#endif

/** Get information about the process
info: initialize to zero before use
flags: enum FFPS_PERF
Return 0 on success */
static int ffps_perf(struct ffps_perf *info, ffuint flags);

/** Get information about the thread
info: initialize to zero before use
Windows: FFPS_PERF_RUSAGE isn't supported
flags: enum FFPS_PERF
Return 0 on success */
static int ffthread_perf(struct ffps_perf *info, ffuint flags);

/** Get the difference between two perf data objects */
static inline void ffps_perf_diff(const struct ffps_perf *start, struct ffps_perf *stop)
{
	fftime_sub(&stop->realtime, &start->realtime);
	fftime_sub(&stop->cputime, &start->cputime);
	fftime_sub(&stop->usertime, &start->usertime);
	fftime_sub(&stop->systime, &start->systime);
	stop->pagefaults -= start->pagefaults;
	stop->maxrss -= start->maxrss;
	stop->inblock -= start->inblock;
	stop->outblock -= start->outblock;
	stop->vctxsw -= start->vctxsw;
	stop->ivctxsw -= start->ivctxsw;
}

/** Add up two perf data objects */
static inline void ffps_perf_add(struct ffps_perf *dst, const struct ffps_perf *src)
{
	fftime_add(&dst->realtime, &src->realtime);
	fftime_add(&dst->cputime, &src->cputime);
	fftime_add(&dst->usertime, &src->usertime);
	fftime_add(&dst->systime, &src->systime);
	dst->pagefaults += src->pagefaults;
	dst->maxrss += src->maxrss;
	dst->inblock += src->inblock;
	dst->outblock += src->outblock;
	dst->vctxsw += src->vctxsw;
	dst->ivctxsw += src->ivctxsw;
}

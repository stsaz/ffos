#include <FFOS/time.h>
#include <FFOS/file.h>
#include <FFOS/dir.h>

#include <sys/fcntl.h>
#include <string.h>

int fffile_nblock(fffd fd, int nblock)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (nblock != 0)
		flags |= O_NONBLOCK;
	else
		flags &= ~O_NONBLOCK;
	return fcntl(fd, F_SETFL, flags);
}

static const ffsyschar * fullPath(ffdirentry *ent, ffsyschar *nm, size_t nmlen) {
	if (ent->pathlen + nmlen + sizeof('/') >= ent->pathcap) {
		errno = EOVERFLOW;
		return NULL;
	}
	ent->path[ent->pathlen] = FFPATH_SLASH;
	memcpy(ent->path + ent->pathlen + 1, nm, nmlen);
	ent->path[ent->pathlen + nmlen + 1] = '\0';
	return ent->path;
}

fffileinfo * ffdir_entryinfo(ffdirentry *ent)
{
	int r;
	const ffsyschar *nm = fullPath(ent, ffdir_entryname(ent), ent->namelen);
	if (nm == NULL)
		return NULL;
	r = fffile_infofn(nm, &ent->info);
	ent->path[ent->pathlen] = '\0';
	return r == 0 ? &ent->info : NULL;
}

void fftime_now(fftime *t)
{
	struct timespec ts = { 0 };
	(void)clock_gettime(CLOCK_REALTIME, &ts);
	fftime_fromtimespec(t, &ts);
}

void fftime_split(ffdtm *dt, const fftime *_t, enum FF_TIMEZONE tz)
{
	const time_t t = _t->s;
	struct tm tt;
	if (tz == FFTIME_TZUTC)
		gmtime_r(&t, &tt);
	else
		localtime_r(&t, &tt);
	fftime_fromtm(dt, &tt);
	dt->msec = _t->mcs / 1000;
}

fftime * fftime_join(fftime *t, const ffdtm *dt, enum FF_TIMEZONE tz)
{
	struct tm tt;
	FF_ASSERT(dt->year >= 1970);
	fftime_totm(&tt, dt);
	if (tz == FFTIME_TZUTC)
		t->s = timegm(&tt);
	else
		t->s = mktime(&tt);
	t->mcs = dt->msec * 1000;
	return t;
}

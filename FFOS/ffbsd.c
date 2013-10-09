#include <FFOS/file.h>

int fffile_time(fffd fd, fftime *last_write, fftime *last_access, fftime *cre)
{
	size_t i;
	fftime *const tt[3] = { last_write, last_access, cre };
	struct stat s;
	const struct timespec *const ts[] = { &s.st_mtim, &s.st_atim, &s.st_ctim };

	if (0 != fstat(fd, &s))
		return -1;

	for (i = 0; i < 3; i++) {
		if (tt[i] != NULL)
			fftime_fromtimespec(tt[i], ts[i]);
	}
	return 0;
}

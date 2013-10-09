#include <FFOS/file.h>
#include <FFOS/time.h>
#include <FFOS/error.h>

#include <time.h>

int fffile_time(fffd fd, fftime *last_write, fftime *last_access, fftime *cre)
{
	struct stat s;
	if (0 != fstat(fd, &s))
		return -1;

	if (last_access)
		fftime_fromtime_t(last_access, s.st_atime);
	if (last_write)
		fftime_fromtime_t(last_write, s.st_mtime);
	if (cre)
		cre->s = cre->mcs = 0;
	return 0;
}

int fferr_str(int code, char *dst, size_t dst_cap) {
	char *dst2;
	if (0 == dst_cap)
		return 0;
	dst2 = strerror_r(code, dst, dst_cap);
	if (dst2 != dst) {
		ssize_t len = ffmin(dst_cap - 1, strlen(dst2));
		memcpy(dst, dst2, len);
		dst[len] = '\0';
		return len;
	}
	return strlen(dst);
}

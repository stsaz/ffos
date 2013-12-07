#include <FFOS/file.h>
#include <FFOS/process.h>

#include <sys/wait.h>


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

int ffps_wait(fffd h, uint timeout, int *exit_code)
{
	int st = 0;
	(void)timeout;

	do {
		if (-1 == waitpid(h, &st, WNOWAIT))
			return -1;
	} while (!WIFEXITED(st) && !WIFSIGNALED(st)); // exited or killed

	if (exit_code != NULL)
		*exit_code = (WIFEXITED(st) ? WEXITSTATUS(st) : -1);

	return 0;
}

/** ffos: volume.h tester
2023, Simon Zolin
*/

#include <FFOS/file.h>
#include <FFOS/dir.h>
#include <FFOS/volume.h>
#include <FFOS/test.h>

void test_vol_mount()
{
#ifdef FF_WIN
	fffd hvol;
	wchar_t buf[512];
	x_sys(FFFILE_NULL != (hvol = FindFirstVolumeW((void*)buf, sizeof(buf))));
	char *volname = ffsz_alloc_wtou(buf);

	const char *dirname = "ffos-mountdir";
	(void) ffdir_make(dirname);

	x_sys(0 == ffvol_mount(volname, dirname));
	x_sys(0 == ffvol_mount(NULL, dirname));

	(void) ffdir_remove(dirname);
	ffmem_free(volname);
	FindVolumeClose(hvol);
#endif
}

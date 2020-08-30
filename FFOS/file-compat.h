#include <FFOS/error.h>
#include <FFOS/std.h>
#ifdef FF_WIN
#include <FFOS/win/file.h>
#include <FFOS/win/fmap.h>
#define ffstd_write  _ffstd_write
#define ffstd_fread(fd, d, len)  ffpipe_read(fd, d, len)
#else
#include <FFOS/unix/file.h>
#include <FFOS/unix/fmap.h>
#define ffstd_write  write
#define ffstd_fread(fd, buf, cap)  fffile_read(fd, buf, cap)
#endif

#ifdef FF_WIN
#define O_CREAT  FFFILE_CREATE
#define O_RDONLY  FFFILE_READONLY
#define O_WRONLY  FFFILE_WRITEONLY
#define O_RDWR  FFFILE_READWRITE
#define O_TRUNC  FFFILE_TRUNCATE
#else
#define FFO_NONBLOCK  FFFILE_NONBLOCK
#endif
#define FFO_CREATE  FFFILE_CREATE
#define FFO_CREATENEW  FFFILE_CREATENEW
#define FFO_APPEND  (FFFILE_CREATE | FFFILE_APPEND)
#define FFO_RDONLY  FFFILE_READONLY
#define FFO_WRONLY  FFFILE_WRITEONLY
#define FFO_RDWR  FFFILE_READWRITE
#define FFO_TRUNC  FFFILE_TRUNCATE
#define FFO_NOATIME  FFFILE_NOATIME
#define FFO_DIRECT  FFFILE_DIRECT
#define FFO_NODOSNAME  FFFILE_NODOSNAME
#define fffile_rm  fffile_remove
#define fffile_infofn  fffile_info_path
#define fffile_infomtime  fffileinfo_mtime
#define fffile_infosize  fffileinfo_size
#define fffile_infoattr  fffileinfo_attr
#define fffile_infoid  fffileinfo_id
#define fffile_nblock  fffile_nonblock
#define fffile_settimefn  fffile_set_mtime_path
#define fffile_settime  fffile_set_mtime
#define fffile_attrsetfn  fffile_set_attr_path
#define fffile_attrset  fffile_set_attr
#define fffile_chown  fffile_set_owner
#define fffile_pwrite  fffile_writeat
#define fffile_pread  fffile_readat


#define fffile_safeclose(fd)  FF_SAFECLOSE(fd, FF_BADFD, fffile_close)

/** Write constant string to a file. */
#define fffile_writecz(fd, csz)  fffile_write(fd, csz, sizeof(csz)-1)


enum FFWIN_FILEATTR {
	FFWIN_FILE_READONLY = 1,
	FFWIN_FILE_HIDDEN = 2,
	FFWIN_FILE_SYSTEM = 4,
	FFWIN_FILE_DIR = 0x10,
	FFWIN_FILE_ARCHIVE = 0x20,
};

// TTTT SSS RWXRWXRWX
enum FFUNIX_FILEATTR {
	FFUNIX_FILE_TYPEMASK = 0170000,
	FFUNIX_FILE_FIFO = 0010000,
	FFUNIX_FILE_CHAR = 0020000,
	FFUNIX_FILE_DIR = 0040000,
	FFUNIX_FILE_BLOCK = 0060000,
	FFUNIX_FILE_REG = 0100000,
	FFUNIX_FILE_LINK = 0120000,
	FFUNIX_FILE_SOCKET = 0140000,
};

/** Convert UNIX file mode to string: "Trwxrwxrwx"
Return number of bytes written. */
FF_EXTN uint fffile_unixattr_tostr(char *dst, size_t cap, uint mode);

/** ffos: standard I/O
2020, Simon Zolin
*/

/*
ffstdin_read
ffstdout_write ffstderr_write
ffstdout_fmt ffstderr_fmt
fflog fflogz
ffstd_keyread
ffstd_keyparse
ffstd_attr
*/

#pragma once

#include <FFOS/string.h>

enum FFKEY {
	FFKEY_VIRT = 1 << 31,
	FFKEY_UP,
	FFKEY_DOWN,
	FFKEY_RIGHT,
	FFKEY_LEFT,

	FFKEY_CTRL = 1 << 24,
	FFKEY_SHIFT = 2 << 24,
	FFKEY_ALT = 4 << 24,
	FFKEY_MODMASK = FFKEY_CTRL | FFKEY_SHIFT | FFKEY_ALT,
};

enum FFSTD_ATTR {
	FFSTD_ECHO = 1, // set echo on/off (stdin)
	FFSTD_LINEINPUT = 2, // set line-input/character-input (stdin)
	FFSTD_VTERM = 4, // Windows: enable control character sequences
};


#define FFSTD_BLACK  "0"
#define FFSTD_RED  "1"
#define FFSTD_GREEN  "2"
#define FFSTD_YELLOW  "3"
#define FFSTD_BLUE  "4"
#define FFSTD_PURPLE  "5"
#define FFSTD_CYAN  "6"
#define FFSTD_WHITE  "7"

#define FFSTD_CLR_RESET  "\033[0m"

/** Normal */
#define FFSTD_CLR(f)      "\033["   "3" f "m"
/** Intense */
#define FFSTD_CLR_I(f)    "\033["   "9" f "m"
/** Bold/bright */
#define FFSTD_CLR_B(f)    "\033[1;" "3" f "m"

/** Background normal */
#define FFSTD_CLRBG(b)    "\033["   "4" b "m"
/** Background intense */
#define FFSTD_CLRBG_I(b)  "\033["  "10" b "m"


#ifdef FF_WIN

#include <ffbase/slice.h>

#define ffstdin  GetStdHandle(STD_INPUT_HANDLE)
#define ffstdout  GetStdHandle(STD_OUTPUT_HANDLE)
#define ffstderr  GetStdHandle(STD_ERROR_HANDLE)

static inline ffssize ffstdin_read(void *buf, ffsize cap)
{
	DWORD read;
	HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
	if (GetConsoleMode(h, &read)) {

		wchar_t ws[1024], *w = ws;
		FF_ASSERT(cap >= 4);
		ffsize wcap = cap / 4;
		if (cap / 4 > FF_COUNT(ws)) {
			if (NULL == (w = (wchar_t*)ffmem_alloc(cap / 2)))
				return -1;
		}

		BOOL b = ReadConsoleW(h, w, ffmin(wcap, 0xffffffff), &read, NULL);

		ffssize r;
		if (b) {
			r = ffs_wtou((char*)buf, cap, w, read);
			if (r < 0)
				b = 0;
		}

		if (w != ws)
			ffmem_free(w);
		return (b) ? r : -1;
	}

	if (!ReadFile(h, buf, ffmin(cap, 0xffffffff), &read, 0))
		return -1;
	return read;
}

static inline ffssize _ffstd_write(HANDLE h, const void *data, ffsize len)
{
	DWORD written;
	if (GetConsoleMode(h, &written)) {
		wchar_t ws[1024], *w;
		ffsize r = FF_COUNT(ws);
		w = ffs_alloc_buf_utow(ws, &r, (char*)data, len);

		BOOL b = WriteConsoleW(h, w, r, &written, NULL);

		if (w != ws)
			ffmem_free(w);
		return (b) ? (ffssize)len : -1;
	}

	if (!WriteFile(h, data, len, &written, 0))
		return -1;
	return written;
}

static inline ffssize ffstdout_write(const void *data, ffsize len)
{
	return _ffstd_write(GetStdHandle(STD_OUTPUT_HANDLE), data, len);
}

static inline ffssize ffstderr_write(const void *data, ffsize len)
{
	return _ffstd_write(GetStdHandle(STD_ERROR_HANDLE), data, len);
}

typedef struct ffstd_ev {
	ffuint state;
	INPUT_RECORD rec[8];
	ffuint irec, nrec;
} ffstd_ev;

static inline int ffstd_keyread(fffd fd, ffstd_ev *ev, ffstr *data)
{
	for (;;) {
		switch (ev->state) {

		case 0: {
			DWORD n;
			if (!GetNumberOfConsoleInputEvents(fd, &n))
				return -1;
			if (n == 0)
				return 0;

			if (!ReadConsoleInput(fd, ev->rec, ffmin(n, FF_COUNT(ev->rec)), &n))
				return -1;
			if (n == 0)
				return 0;
			ev->nrec = n;
			ev->irec = 0;
			ev->state = 1;
		}
		// fallthrough

		case 1:
			if (ev->irec == ev->nrec) {
				ev->state = 0;
				continue;
			}

			if (!(ev->rec[ev->irec].EventType == KEY_EVENT && ev->rec[ev->irec].Event.KeyEvent.bKeyDown)) {
				ev->irec++;
				continue;
			}

			ffstr_set(data, (void*)&ev->rec[ev->irec].Event.KeyEvent, sizeof(ev->rec[ev->irec].Event.KeyEvent));
			ev->irec++;
			return data->len;
		}
	}
}

static inline int ffstd_keyparse(ffstr *data)
{
	if (data->len < sizeof(KEY_EVENT_RECORD))
		return -1;

	const KEY_EVENT_RECORD *k = (KEY_EVENT_RECORD*)data->ptr;
	ffuint r = k->uChar.AsciiChar;
	if (r == 0) {
		// VK_* -> FFKEY_*
		static const ffbyte vkeys[] = {
			VK_LEFT, VK_UP, VK_RIGHT, VK_DOWN,
		};
		static const ffbyte ffkeys[] = {
			(ffbyte)FFKEY_LEFT, (ffbyte)FFKEY_UP, (ffbyte)FFKEY_RIGHT, (ffbyte)FFKEY_DOWN,
		};
		if ((ffuint)-1 == (r = ffarrint8_binfind(vkeys, FF_COUNT(vkeys), k->wVirtualKeyCode)))
			return -1;
		r = FFKEY_VIRT | ffkeys[r];
	}

	ffuint ctl = k->dwControlKeyState;
	if (ctl & (RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED))
		r |= FFKEY_ALT;
	if (ctl & (RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED))
		r |= FFKEY_CTRL;
	if (ctl & SHIFT_PRESSED)
		r |= FFKEY_SHIFT;

	data->len = 0;
	return r;
}

static inline int ffstd_attr(fffd fd, ffuint attr, ffuint val)
{
	DWORD mode;
	if (!GetConsoleMode(fd, &mode))
		return -1;

	if ((attr & FFSTD_VTERM) && (val & FFSTD_VTERM)) {
		mode |= ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	}

	if (attr & FFSTD_ECHO) {
		if (val & FFSTD_ECHO)
			mode |= ENABLE_ECHO_INPUT;
		else
			mode &= ~ENABLE_ECHO_INPUT;
	}

	if (attr & FFSTD_LINEINPUT) {
		if (val & FFSTD_LINEINPUT)
			mode |= ENABLE_LINE_INPUT;
		else
			mode &= ~ENABLE_LINE_INPUT;
	}

	return !SetConsoleMode(fd, mode);
}

#else // UNIX:

#include <FFOS/error.h>
#include <termios.h>

#define ffstdin  0
#define ffstdout  1
#define ffstderr  2

static inline ffssize ffstdin_read(void *buf, ffsize cap)
{
	return read(0, buf, cap);
}

static inline ffssize ffstdout_write(const void *data, ffsize len)
{
	return write(1, data, len);
}

static inline ffssize ffstderr_write(const void *data, ffsize len)
{
	return write(2, data, len);
}

typedef struct ffstd_ev {
	char buf[8];
} ffstd_ev;

static inline int ffstd_keyread(fffd fd, ffstd_ev *ev, ffstr *data)
{
	ffssize r = read(fd, ev->buf, sizeof(ev->buf));
	if (r < 0 && fferr_again(errno))
		return 0;
	else if (r == 0) {
		errno = EINVAL;
		return -1;
	}
	ffstr_set(data, ev->buf, r);
	return r;
}

static inline int ffstd_keyparse(ffstr *data)
{
	int r = 0, i = 0;
	const char *d = data->ptr;
	if (data->len == 0)
		return -1;

	if (d[0] == '\x1b' && data->len >= 3 && d[1] == '[') {

		if (d[2] == '1' && data->len > 5 && d[3] == ';') {
			ffuint m = (ffbyte)d[4];

			// ESC [ 1;N
			static const ffuint key_esc1[] = {
				FFKEY_SHIFT /*N == 2*/,
				FFKEY_ALT,
				FFKEY_SHIFT | FFKEY_ALT,
				FFKEY_CTRL,
				FFKEY_CTRL | FFKEY_SHIFT,
				FFKEY_CTRL | FFKEY_ALT,
				FFKEY_CTRL | FFKEY_SHIFT | FFKEY_ALT,
			};

			if (m - '2' >= FF_COUNT(key_esc1))
				return -1;
			r = key_esc1[m - '2'];
			i += FFS_LEN("1;N");
		}
		i += 2;

		if (d[i] >= 'A' && d[i] <= 'D') {
			r |= FFKEY_UP + d[i] - 'A';
			ffstr_shift(data, i + 1);
			return r;
		}

		return -1;
	}

	ffstr_shift(data, 1);
	return d[0];
}


static inline int ffstd_attr(fffd fd, ffuint attr, ffuint val)
{
	if (attr == FFSTD_VTERM)
		return 0; // enabled by default

	struct termios t;
	if (0 != tcgetattr(fd, &t))
		return -1;

	if (attr & FFSTD_ECHO) {
		if (val & FFSTD_ECHO)
			t.c_lflag |= ECHO;
		else
			t.c_lflag &= ~ECHO;
	}

	if (attr & FFSTD_LINEINPUT) {
		if (val & FFSTD_LINEINPUT) {
			t.c_lflag |= ICANON;
		} else {
			t.c_lflag &= ~ICANON;
			t.c_cc[VTIME] = 0;
			t.c_cc[VMIN] = 1;
		}
	}

	tcsetattr(fd, TCSANOW, &t);
	return 0;
}

#endif


/** Read from stdin.
Windows: 'cap' must be >=4.
Return N of bytes read */
static ffssize ffstdin_read(void *buf, ffsize cap);

/** Write to stdout */
static ffssize ffstdout_write(const void *data, ffsize len);

/** Write to stderr */
static ffssize ffstderr_write(const void *data, ffsize len);

/** %-formatted output to stdout
NOT printf()-compatible (see ffs_formatv()) */
static inline ffssize ffstdout_fmt(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	ffstr s = {};
	ffsize cap = 0;
	ffsize r = ffstr_growfmtv(&s, &cap, fmt, args);
	va_end(args);
	if (r != 0)
		r = ffstdout_write(s.ptr, r);
	ffstr_free(&s);
	return r;
}

/** %-formatted output to stderr
NOT printf()-compatible (see ffs_formatv()) */
static inline ffssize ffstderr_fmt(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	ffstr s = {};
	ffsize cap = 0;
	ffsize r = ffstr_growfmtv(&s, &cap, fmt, args);
	va_end(args);
	if (r != 0)
		r = ffstderr_write(s.ptr, r);
	ffstr_free(&s);
	return r;
}

/** Write %-formatted text line to stdout */
#define fflog(fmt, ...)  (void) ffstdout_fmt(fmt "\n", ##__VA_ARGS__)

/** Write text line to stdout */
#define fflogz(sz)  (void) ffstdout_fmt("%s\n", sz)

/** Read keyboard event from terminal
fd: usually ffstdin
ev: initialize to {} on very first use
data: the data read from stdin
Return N of bytes read on success
  0 if queue is empty
  <0 on error */
static int ffstd_keyread(fffd fd, ffstd_ev *ev, ffstr *data);

/** Parse key received from terminal
Return enum FFKEY;  'data' is shifted by the number of bytes processed
  <0 on error */
static int ffstd_keyparse(ffstr *data);


/** Set attribute on a terminal
attr: enum FFSTD_ATTR
val: enum FFSTD_ATTR
Return !=0 on error */
static int ffstd_attr(fffd fd, ffuint attr, ffuint val);

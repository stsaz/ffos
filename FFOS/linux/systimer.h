/** Disable system sleep timer on Linux via D-BUS.
Copyright (c) 2020 Simon Zolin
*/

struct ffps_systimer {
	const char *appname;
	const char *reason;
	void *conn;
	void *reply;
};

/** Open D-BUS connection. */
FF_EXTN int ffps_systimer_open(struct ffps_systimer *c);

/** Close D-BUS connection. */
FF_EXTN void ffps_systimer_close(struct ffps_systimer *c);

/**
flags: 1:inhibit screen saver;  0:uninhibit screen saver */
FF_EXTN int ffps_systimer(struct ffps_systimer *c, uint flags);

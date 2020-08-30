/**
Copyright (c) 2017 Simon Zolin
*/

#include <FFOS/win/reg.h>
#include <FFOS/test.h>

void test_winreg()
{
	ffwreg k;
	ffwreg_val val;

	x_sys(FFWINREG_NULL != (k = ffwinreg_open(HKEY_CURRENT_USER, "Software\\ffostest", FFWINREG_CREATE | FFWINREG_READWRITE)));

	// String
	val.type = REG_SZ;
	val.data = "Value";
	val.datalen = FFSLEN("Value");
	x(0 == ffwreg_write(k, "NameStr", &val));

	memset(&val, 0, sizeof(val));
	x(1 == ffwreg_read(k, "NameStr", &val));
	x(val.type == REG_SZ);
	x(val.datalen == FFSLEN("Value"));
	x(!strncmp(val.data, "Value", FFSLEN("Value")));
	ffmem_free(val.data);

	// Integer
	uint n = 12345;
	val.type = REG_DWORD;
	val.data = (void*)&n;
	val.datalen = sizeof(n);
	x(0 == ffwreg_write(k, "NameInt", &val));

	memset(&val, 0, sizeof(val));
	x(1 == ffwreg_read(k, "NameInt", &val));
	x(val.type == REG_DWORD);
	x(val.datalen == sizeof(int));
	x(*(uint*)val.data == n);
	ffmem_free(val.data);

#if FF_WIN >= 0x0600
	x_sys(0 == ffwreg_del(k, "", "NameStr"));
	x_sys(0 == ffwreg_del(k, "", "NameInt"));
	x_sys(0 == ffwreg_del(k, "", NULL));
#endif

	ffwreg_close(k);
}

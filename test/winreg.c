/**
Copyright (c) 2017 Simon Zolin
*/

#include <FFOS/win/reg.h>
#include <FFOS/test.h>

void test_winreg()
{
	ffwinreg k;
	ffwinreg_val val;
	char buf[64];

	x_sys(FFWINREG_NULL != (k = ffwinreg_open(HKEY_CURRENT_USER, "Software\\ffostest", FFWINREG_CREATE | FFWINREG_READWRITE)));

	// String
	val.type = REG_SZ;
	val.data = "Value";  val.datalen = FFS_LEN("Value");
	x(0 == ffwinreg_write(k, "NameStr", &val));

	ffmem_zero_obj(&val);
	x(1 == ffwinreg_read(k, "NameStr", &val));
	xieq(val.type, REG_SZ);
	xieq(val.datalen, FFS_LEN("Value"));
	x(!strncmp(val.data, "Value", FFS_LEN("Value")));
	ffmem_free(val.data);

	ffmem_zero_obj(&val);
	val.data = buf;  val.datalen = sizeof(buf);
	xieq(1, ffwinreg_read(k, "NameStr", &val));
	xieq(val.type, REG_SZ);
	x(val.data == buf);
	xieq(val.datalen, FFS_LEN("Value"));
	x(!strncmp(val.data, "Value", FFS_LEN("Value")));

	ffmem_zero_obj(&val);
	val.data = buf;  val.datalen = 5;
	xieq(1, ffwinreg_read(k, "NameStr", &val));
	xieq(val.type, REG_SZ);
	x(val.data == buf);
	xieq(val.datalen, FFS_LEN("Value"));
	x(!strncmp(val.data, "Value", FFS_LEN("Value")));

	ffmem_zero_obj(&val);
	val.data = buf;  val.datalen = 4;
	x(0 == ffwinreg_read(k, "NameStr", &val));
	xieq(val.type, REG_SZ);
	x(val.data == buf);
	xieq(val.datalen, FFS_LEN("Value")*4+4);

	// Integer
	ffuint n = 12345;
	val.type = REG_DWORD;
	val.data = (void*)&n;  val.datalen = sizeof(n);
	x(0 == ffwinreg_write(k, "NameInt", &val));

	ffmem_zero_obj(&val);
	x(1 == ffwinreg_read(k, "NameInt", &val));
	x(val.type == REG_DWORD);
	x(val.datalen == sizeof(int));
	x(*(ffuint*)val.data == n);
	ffmem_free(val.data);

	ffmem_zero_obj(&val);
	val.data = (void*)&n;  val.datalen = sizeof(n);
	x(1 == ffwinreg_read(k, "NameInt", &val));
	x(val.type == REG_DWORD);
	x(val.datalen == sizeof(int));
	x(*(ffuint*)val.data == 12345);

	ffmem_zero_obj(&val);
	val.data = (void*)&n;  val.datalen = sizeof(n)-1;
	x(0 == ffwinreg_read(k, "NameInt", &val));
	x(val.type == REG_DWORD);
	x(val.data == (void*)&n);
	x(val.datalen == sizeof(int));

#if FF_WIN >= 0x0600
	x_sys(0 == ffwinreg_del(k, "", "NameStr"));
	x_sys(0 == ffwinreg_del(k, "", "NameInt"));
	x_sys(0 == ffwinreg_del(k, "", NULL));
#endif

	ffwinreg_close(k);
}

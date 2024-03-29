/**
Copyright (c) 2017 Simon Zolin
*/

#include <FFOS/winreg.h>
#include <FFOS/test.h>

void test_winreg_enum(ffwinreg k)
{
	ffwinreg k2;
	x_sys(FFWINREG_NULL != (k2 = ffwinreg_open(HKEY_CURRENT_USER, "Software\\ffostest\\subkey", FFWINREG_CREATE | FFWINREG_READWRITE)));

	ffwinreg_enum e = {};
	ffwinreg_enum_init(&e, FFWINREG_ENUM_VALSTR);

	ffstr name, val;
	ffuint t;

	ffwinreg_enum_begin(&e, k);
	x_sys(0 == ffwinreg_enum_nextkey(&e, &name));
	xseq(&name, "subkey");
	x_sys(1 == ffwinreg_enum_nextkey(&e, &name));

	ffwinreg_enum_begin(&e, k);
	int n = 0;
	for (;;) {
		int r = ffwinreg_enum_nextval(&e, &name, &val, &t);
		if (r == 1)
			break;
		x_sys(r == 0);

		if (ffstr_eqz(&name, "NameInt")) {
			xseq(&val, "0x00003039 (12345)");
			x(t == REG_DWORD);
			n++;

		} else if (ffstr_eqz(&name, "NameStr")) {
			xseq(&val, "Value");
			x(t == REG_SZ);
			n++;
		}
	}
	xieq(n, 2);

	ffwinreg_enum_destroy(&e);

#if FF_WIN >= 0x0600
	x_sys(0 == ffwinreg_del(k2, "", NULL));
#endif
}

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

	test_winreg_enum(k);

#if FF_WIN >= 0x0600
	x_sys(0 == ffwinreg_del(k, "", "NameStr"));
	x_sys(0 == ffwinreg_del(k, "", "NameInt"));
	x_sys(0 == ffwinreg_del(k, "", NULL));
#endif

	ffwinreg_close(k);
}

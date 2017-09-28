/**
Copyright (c) 2013 Simon Zolin
*/

#include <FFOS/types.h>
#include <FFOS/test.h>

#define x FFTEST_BOOL

struct struct1 {
	int n1;
	int n2;
};

int test_types() {
	FFTEST_FUNC;

	x(sizeof(byte) == 1);
	x(sizeof(ushort) == 2);
	x(sizeof(uint) == 4);
#ifdef FF_64
	x(sizeof(size_t) == 8);
#else
	x(sizeof(size_t) == 4);
#endif
	x(sizeof(uint64) == 8);

	x(FFOFF(struct struct1, n1) == 0);
	x(FFOFF(struct struct1, n2) == sizeof(int));

	x(ffmin(1, 2) == 1);
	x(ffmin(2, 1) == 1);

	x(ffhton32(0x12345678) == 0x78563412);

	x(ffint_bswap16(0x1234) == 0x3412);
	x(ffint_bswap32(0x12345678) == 0x78563412);
	x(ffint_bswap64(0x12345678abcdef12) == 0x12efcdab78563412);

	return 0;
}

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

	x(0xfff0 == ff_align_floor2(0xffff, 0x10));
	x(0x123456780 == ff_align_floor2((uint64)0x123456789, (uint)0x10));
	x(0x10000 == ff_align_ceil2(0xffff, 0x10));
	x(10 == ff_align_floor(11, 5));
	x(15 == ff_align_ceil(11, 5));

	x(0x8000000000000000ULL == ff_align_power2(0x8000000000000000ULL));
	x(32 == ff_align_power2(17));
	x(16 == ff_align_power2(16));
	x(16 == ff_align_power2(15));
	x(2 == ff_align_power2(2));
	x(1 == ff_align_power2(1));

	x(ffhton32(0x12345678) == 0x78563412);

	x(ffint_bswap16(0x1234) == 0x3412);
	x(ffint_bswap32(0x12345678) == 0x78563412);
	x(ffint_bswap64(0x12345678abcdef12) == 0x12efcdab78563412);

	int v = 1;
	x(FF_CMPSET(&v, 1, 2) && v == 2);
	x(!FF_CMPSET(&v, 1, 0) && v == 2);

	return 0;
}

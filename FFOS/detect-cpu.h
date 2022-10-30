/** Preprocessor detection of CPU.
Copyright (c) 2020 Simon Zolin

CPU types:
. FF_AMD64
. FF_X86
. FF_ARM

FF_64 - set if CPU supports 64-bit instruction set.
*/


#if defined __amd64__ || defined _M_AMD64
	#define FF_AMD64
	#define FF_LITTLE_ENDIAN

#elif defined __i386__ || defined _M_IX86
	#define FF_X86
	#define FF_LITTLE_ENDIAN

#elif defined __arm__ || defined _M_ARM || defined __aarch64__
	#define FF_ARM

	#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
		#define FF_LITTLE_ENDIAN
	#else
		#define FF_BIG_ENDIAN
	#endif

#else
	#warning "This CPU is not supported.  Assuming big-endian."
	#define FF_BIG_ENDIAN
#endif

#if defined __LP64__ || defined _WIN64
	#define FF_64
#endif

/** Preprocessor detection of compiler.
Copyright (c) 2020 Simon Zolin

Compiler types:
. FF_CLANG
. FF_MINGW
. FF_GCC
*/


#if defined __clang__
	#define FF_CLANG

#elif defined __MINGW32__ || defined __MINGW64__
	#define FF_MINGW

#elif defined __GNUC__
	#define FF_GCC

// #elif defined _MSC_VER
// 	#define FF_MSVC

#else
	#error "This compiler is not supported"
#endif

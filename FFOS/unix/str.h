/**
Copyright (c) 2013 Simon Zolin
*/

#include <string.h>

#ifndef _S
	/** Native string. */
	#define _S(s)  s
#endif

#define FF_NEWLN  "\n"

#define ffq_len  strlen
#define ffq_cmp2  strcmp
#define ffq_icmp2  stricmp
#define ffq_icmp  strncasecmp
#define ffq_cpy2  strcpy
#define ffq_cat2  strcat

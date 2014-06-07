/**
Copyright (c) 2013 Simon Zolin
*/

#include <string.h>

#if !defined TEXT
	#define TEXT(s)  s
#endif

#define FF_NEWLN  "\n"

#define ffq_len  strlen
#define ffq_cmpz  strcmp
#define ffq_icmpz  strcasecmp
#define ffq_icmpnz  strncasecmp
#define ffq_cpy2  strcpy
#define ffq_cat2  strcat

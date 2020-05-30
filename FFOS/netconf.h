/** Network configuration
Copyright (c) 2020 Simon Zolin
*/

#pragma once

#include <FFOS/mem.h>


enum FFNETCONF_F {
	FFNETCONF_DNS_ADDR = 1, // list of DNS server addresses
};

typedef struct {
	char **dns_addrs; // char*[]
	uint dns_addrs_num;
} ffnetconf;

static inline void ffnetconf_destroy(ffnetconf *nc)
{
	ffmem_free0(nc->dns_addrs);
}

#ifdef FF_UNIX

/** Get system network configuration
flags: enum FFNETCONF_F
Return 0 on success */
static inline int ffnetconf_get(ffnetconf *nc, uint flags);

#include <FFOS/unix/nconf.h>

#elif defined FF_WIN

FF_EXTN int ffnetconf_get(ffnetconf *nc, uint flags);

#endif

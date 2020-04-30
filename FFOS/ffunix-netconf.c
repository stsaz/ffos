/**
Copyright (c) 2020 Simon Zolin
*/

#include <FFOS/socket.h>
#include <FFOS/mem.h>


void ffnetconf_destroy(ffnetconf *nc)
{
	ffmem_free0(nc->dns_addrs);
}

extern int _ffnetconf_getdns(ffnetconf *nc);

int ffnetconf_get(ffnetconf *nc, uint flags)
{
	int rc = -1;
	switch (flags) {
	case FFNETCONF_DNS_ADDR: {
		rc = _ffnetconf_getdns(nc);
		break;
	}
	}
	return rc;
}

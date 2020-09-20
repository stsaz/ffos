/** ffos: network configuration
2020, Simon Zolin
*/

/*
ffnetconf_destroy
ffnetconf_get
*/

#pragma once

#include <FFOS/base.h>

enum FFNETCONF_F {
	FFNETCONF_DNS_ADDR = 1, // list of DNS server addresses
};

typedef struct {
	char **dns_addrs; // char*[]
	ffuint dns_addrs_num;
} ffnetconf;

#ifdef FF_WIN

#include <iphlpapi.h>

static FIXED_INFO* _ffnetconf_info()
{
	FIXED_INFO *info = NULL;
	DWORD sz = sizeof(FIXED_INFO);
	ffbool first = 1;
	for (;;) {
		if (NULL == (info = (FIXED_INFO*)ffmem_alloc(sz)))
			return NULL;
		ffuint r = GetNetworkParams(info, &sz);
		if (r == ERROR_BUFFER_OVERFLOW) {
			if (!first)
				goto end;
			first = 0;
			ffmem_free(info);
			info = NULL;
			continue;
		} else if (r != 0)
			goto end;
		break;
	}
	return info;

end:
	ffmem_free(info);
	return NULL;
}

/** Get DNS server addresses */
static int _ffnetconf_dns(ffnetconf *nc, const FIXED_INFO *info)
{
	const IP_ADDR_STRING *ip;
	ffsize cap = 0;
	ffuint n = 0;
	for (ip = &info->DnsServerList;  ip != NULL;  ip = ip->Next) {
		cap += ffsz_len(ip->IpAddress.String) + 1;
		n++;
	}
	if (NULL == (nc->dns_addrs = (char**)ffmem_alloc(cap + n * sizeof(void*))))
		return -1;

	// ptr1 ptr2 ... data1 data2 ...
	char **ptr = nc->dns_addrs;
	char *data = (char*)nc->dns_addrs + n * sizeof(void*);
	for (ip = &info->DnsServerList;  ip != NULL;  ip = ip->Next) {
		ffsize len = ffsz_len(ip->IpAddress.String) + 1;
		ffmem_copy(data, ip->IpAddress.String, len);
		*(ptr++) = data;
		data += len;
	}
	nc->dns_addrs_num = n;
	return 0;
}

static inline int ffnetconf_get(ffnetconf *nc, ffuint flags)
{
	int rc = -1;
	switch (flags) {
	case FFNETCONF_DNS_ADDR: {
		FIXED_INFO *info = _ffnetconf_info();
		if (info == NULL)
			break;

		rc = _ffnetconf_dns(nc, info);

		ffmem_free(info);
		break;
	}
	}

	return rc;
}

#else // UNIX:

#include <ffbase/vector.h>
#include <fcntl.h>

/** Read system DNS configuration file and get DNS server addresses */
static int _ffnetconf_dns(ffnetconf *nc)
{
	int rc = -1, fd = -1;
	ffstr fdata = {};
	ffvec servers = {}; //ffstr[]
	ffsize cap = 4*1024;
	char **ptr;
	char *data;
	const char *fn = "/etc/resolv.conf";
	ffstr in, line, word, skip = FFSTR_INIT(" \t\r"), *el;

	if (NULL == ffstr_alloc(&fdata, cap))
		goto end;

	if (-1 == (fd = open(fn, O_RDONLY)))
		goto end;

	ffssize n;
	if (0 >= (n = read(fd, fdata.ptr, cap)))
		goto end;
	fdata.len = n;

	cap = 0;
	in = fdata;
	while (in.len != 0) {
		ffstr_splitby(&in, '\n', &line, &in);
		ffstr_skipany(&line, &skip);

		ffstr_splitbyany(&line, " \t", &word, &line);
		if (ffstr_eqz(&word, "nameserver")) {
			ffstr_splitbyany(&line, " \t", &word, NULL);
			if (NULL == (el = ffvec_pushT(&servers, ffstr)))
				goto end;
			*el = word;
			cap += word.len + 1;
		}
	}

	if (NULL == (nc->dns_addrs = (char**)ffmem_alloc(cap + servers.len * sizeof(void*))))
		goto end;

	// ptr1 ptr2 ... data1 data2 ...
	ptr = nc->dns_addrs;
	data = (char*)nc->dns_addrs + servers.len * sizeof(void*);
	FFSLICE_WALK_T(&servers, el, ffstr) {
		ffmem_copy(data, el->ptr, el->len);
		*(ptr++) = data;
		data += el->len;
		*(data++) = '\0';
	}
	nc->dns_addrs_num = servers.len;
	rc = 0;

end:
	close(fd);
	ffvec_free(&servers);
	ffstr_free(&fdata);
	return rc;
}

static inline int ffnetconf_get(ffnetconf *nc, ffuint flags)
{
	int rc = -1;
	switch (flags) {
	case FFNETCONF_DNS_ADDR:
		rc = _ffnetconf_dns(nc);
		break;
	}
	return rc;
}

#endif


/** Destroy ffnetconf object */
static inline void ffnetconf_destroy(ffnetconf *nc)
{
	ffmem_free(nc->dns_addrs);
	nc->dns_addrs = NULL;
}

/** Get system network configuration
flags: enum FFNETCONF_F
Return 0 on success */
static int ffnetconf_get(ffnetconf *nc, ffuint flags);

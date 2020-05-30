/** ffos: netconf functions for UNIX
Copyright (c) 2020 Simon Zolin
*/

#include <ffbase/string.h>
#include <ffbase/vector.h>


/** Read system DNS configuration file and get DNS server addresses */
static inline int _ffnetconf_getdns(ffnetconf *nc)
{
	int rc = -1, fd = FF_BADFD;
	ffstr fdata = {};
	ffvec servers = {}; //ffstr[]
	ffsize cap = 4*1024;

	if (NULL == ffstr_alloc(&fdata, cap))
		goto end;

	const char *fn = "/etc/resolv.conf";
	if (FF_BADFD == (fd = fffile_open(fn, O_RDONLY)))
		goto end;

	ffssize n;
	if (0 >= (n = fffile_read(fd, fdata.ptr, cap)))
		goto end;
	fdata.len = n;

	cap = 0;
	ffstr in, line, word, skip = FFSTR_INIT(" \t\r"), *el;
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

	if (NULL == (nc->dns_addrs = ffmem_alloc(cap + servers.len * sizeof(void*))))
		goto end;

	// ptr1 ptr2 ... data1 data2 ...
	char **ptr = nc->dns_addrs;
	char *data = (char*)nc->dns_addrs + servers.len * sizeof(void*);
	FFSLICE_WALK_T(&servers, el, ffstr) {
		ffmem_copy(data, el->ptr, el->len);
		*(ptr++) = data;
		data += el->len;
		*(data++) = '\0';
	}
	nc->dns_addrs_num = servers.len;
	rc = 0;

end:
	fffile_close(fd);
	ffvec_free(&servers);
	ffstr_free(&fdata);
	return rc;
}

static inline int ffnetconf_get(ffnetconf *nc, ffuint flags)
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

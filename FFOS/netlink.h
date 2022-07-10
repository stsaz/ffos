/** ffos: netlink
2022, Simon Zolin
*/

/*
ffnetlink_create ffnetlink_close
ffnetlink_send ffnetlink_recv
ffnetlink_next
ffnetlink_rtattr_next
*/

#pragma once
#include <FFOS/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

/**
groups:
	proto=NETLINK_ROUTE: RTMGRP_*
flags: SOCK_NONBLOCK
Return FFSOCK_NULL on error */
static inline ffsock ffnetlink_create(ffuint proto, ffuint groups, ffuint flags)
{
	ffsock nl;
	if (-1 == (nl = socket(AF_NETLINK, SOCK_DGRAM | flags, proto)))
		return -1;

	struct sockaddr_nl sa = {};
	sa.nl_family = AF_NETLINK;
	sa.nl_groups = groups;
	if (0 != bind(nl, (struct sockaddr*)&sa, sizeof(sa))) {
		close(nl);
		return -1;
	}

	return nl;
}

static inline void ffnetlink_close(ffsock nl)
{
	if (nl == -1)
		return;
	close(nl);
}

/** Prepare header */
static ffuint _ffnetlink_hdr_set(struct nlmsghdr *h, ffuint type, ffuint body_len, ffuint seq)
{
	ffmem_zero_obj(h);
	h->nlmsg_len = NLMSG_LENGTH(body_len);
	h->nlmsg_type = type;
	h->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	h->nlmsg_seq = seq;
	return h->nlmsg_len;
}

static void _ffnetlink_msg_set(struct msghdr *msg, struct sockaddr_nl *sa, ffiovec *iov, ffuint iov_n)
{
	ffmem_zero_obj(sa);
	sa->nl_family = AF_NETLINK;

	ffmem_zero_obj(msg);
	msg->msg_name = sa;
	msg->msg_namelen = sizeof(*sa);
	msg->msg_iov = iov;
	msg->msg_iovlen = iov_n;
}

/** Send request
type -> data:
	RTM_GETLINK, RTM_GETADDR: struct rtgenmsg
*/
static inline int ffnetlink_send(ffsock nl, ffuint type, const void *d, ffsize n, ffuint seq)
{
	struct nlmsghdr h;
	_ffnetlink_hdr_set(&h, type, n, seq);

	ffiovec iov[2];
	ffiovec_set(&iov[0], &h, sizeof(h));
	ffiovec_set(&iov[1], d, n);

	struct sockaddr_nl sa;
	struct msghdr msg;
	_ffnetlink_msg_set(&msg, &sa, iov, 2);

	if (sizeof(h) + n != (ffuint)sendmsg(nl, &msg, 0))
		return -1;
	return 0;
}

/** Receive response */
static inline int ffnetlink_recv(ffsock nl, void *d, ffsize n)
{
	struct sockaddr_nl sa;
	struct msghdr msg;
	ffiovec iov;
	ffiovec_set(&iov, d, n);
	_ffnetlink_msg_set(&msg, &sa, &iov, 1);
	int r = recvmsg(nl, &msg, 0);
	if (r <= 0)
		return r;
	if (msg.msg_flags & MSG_TRUNC) {
		errno = EMSGSIZE;
		return -1;
	}
	return r;
}

/** Get next 'nlmsghdr' object.
"nlmsghdr.nlmsg_type -> data" table:
	RTM_NEWLINK:
		(struct ifinfomsg) ((struct rtattr) DATA)...
			rtattr.rta_type: IFLA_*
	RTM_NEWADDR, RTM_DELADDR:
		(struct ifaddrmsg) ((struct rtattr) DATA)...
			rtattr.rta_type: IFA_*
	NLMSG_ERROR:
		struct nlmsgerr
	NLMSG_DONE - message is finished
Return NULL if no more objects. */
static inline struct nlmsghdr* ffnetlink_next(ffstr *buf, ffstr *body)
{
	struct nlmsghdr *nh = (struct nlmsghdr*)buf->ptr;
	if (!NLMSG_OK(nh, buf->len))
		return NULL;
	buf->ptr = (char*)NLMSG_NEXT(nh, buf->len);
	ffstr_set(body, NLMSG_DATA(nh), NLMSG_PAYLOAD(nh, 4));
	return nh;
}

/** Get next 'rtattr' object.
Return NULL if no more objects. */
static inline struct rtattr* ffnetlink_rtattr_next(ffstr *buf, ffstr *body)
{
	struct rtattr *a = (struct rtattr*)buf->ptr;
	if (!RTA_OK(a, buf->len))
		return NULL;
	buf->ptr = (char*)RTA_NEXT(a, buf->len);
	ffstr_set(body, RTA_DATA(a), RTA_PAYLOAD(a));
	return a;
}

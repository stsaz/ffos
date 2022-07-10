/** ffos: netlink tester
2022, Simon Zolin
*/

#include <FFOS/netlink.h>
#include <FFOS/test.h>

void test_netlink()
{
	ffsock nl;
	x_sys(FFSOCK_NULL != (nl = ffnetlink_create(NETLINK_ROUTE, RTMGRP_LINK | RTMGRP_NOTIFY | RTMGRP_IPV4_IFADDR, 0)));

	struct rtgenmsg gen;
	void *buf = ffmem_alloc(1*1024*1024);
	ffstr d, dd, da;
	struct nlmsghdr *nh;
	struct rtattr *attr;

	int st = 0, n;
	for (;;) {
	switch (st) {
	case 0:
		gen.rtgen_family = AF_UNSPEC;
		x_sys(0 == ffnetlink_send(nl, RTM_GETLINK, &gen, sizeof(struct rtgenmsg), 1));
		fflog("sendmsg RTM_GETLINK");
		st++;
		break;

	case 2:
		gen.rtgen_family = AF_UNSPEC;
		x_sys(0 == ffnetlink_send(nl, RTM_GETADDR, &gen, sizeof(struct rtgenmsg), 2));
		fflog("sendmsg RTM_GETADDR");
		st++;
		break;

	default:
		n = ffnetlink_recv(nl, buf, 1*1024*1024);
		x_sys(n > 0);
		ffstr_set(&d, buf, n);

		while (NULL != (nh = ffnetlink_next(&d, &dd))) {
			fflog("nh %d: %d", nh->nlmsg_type, nh->nlmsg_len);
			switch (nh->nlmsg_type) {
			case RTM_NEWLINK: {
				const struct ifinfomsg *ifi = (struct ifinfomsg*)dd.ptr;
				fflog(" ifi_family:\t%d", ifi->ifi_family);
				fflog(" ifi_type:\t%xu", ifi->ifi_type);
				fflog(" ifi_index:\t%d", ifi->ifi_index);
				fflog(" ifi_flags:\t%xu", ifi->ifi_flags);
				fflog(" ifi_change:\t%xu", ifi->ifi_change);

				ffstr_shift(&dd, sizeof(struct ifinfomsg));
				while (NULL != (attr = ffnetlink_rtattr_next(&dd, &da))) {
					// fflog(" rta %d: %d", attr->rta_type, attr->rta_len);
					switch (attr->rta_type) {
					case IFLA_IFNAME:
						fflog("  IFLA_IFNAME:\t%s", da.ptr);
						break;
					case IFLA_ADDRESS:
						fflog("  IFLA_ADDRESS:\t%*xb", da.len, da.ptr);
						break;
					}
				}
				break;
			}

			case RTM_NEWADDR:
			case RTM_DELADDR: {
				struct ifaddrmsg *ifa = (struct ifaddrmsg*)dd.ptr;
				fflog(" ifa_index:\t%d", ifa->ifa_index);
				fflog(" ifa_family:\t%d", ifa->ifa_family);

				ffstr_shift(&dd, sizeof(struct ifaddrmsg));
				while (NULL != (attr = ffnetlink_rtattr_next(&dd, &da))) {
					// fflog(" rta %d: %d", attr->rta_type, attr->rta_len);
					switch (attr->rta_type) {
					case IFA_ADDRESS:
						fflog("  IFA_ADDRESS:\t%*xb", da.len, da.ptr);
						break;
					case IFA_LOCAL:
						fflog("  IFA_LOCAL:\t%*xb", da.len, da.ptr);
						break;
					case IFA_BROADCAST:
						fflog("  IFA_BROADCAST:\t%*xb", da.len, da.ptr);
						break;
					}
				}
				break;
			}

			case NLMSG_ERROR: {
				struct nlmsgerr *e = (struct nlmsgerr*)dd.ptr;
				if (dd.len >= sizeof(struct nlmsgerr)) {
					fflog("error: %E", -e->error);
				}
				break;
			}

			case NLMSG_DONE:
				st++;
				goto brk;
			}
		}
brk:
		break;
	}
	}

// end:
	ffmem_free(buf);
	ffnetlink_close(nl);
}

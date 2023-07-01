#ifndef RELIB_IP_INFO_H
#define RELIB_IP_INFO_H

#include "relib/s/ip_addr.h"

struct ip_info {
	struct ip_addr ip;
	struct ip_addr netmask;
	struct ip_addr gw;
};
typedef struct ip_info ip_info_st;

static_assert(sizeof(ip_info_st) == 12, "ip_info size mismatch");

#endif /* RELIB_IP_INFO_H */

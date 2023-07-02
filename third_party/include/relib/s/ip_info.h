#ifndef RELIB_IP_INFO_H
#define RELIB_IP_INFO_H

#include "relib/s/ip_addr.h"

#if LIBLWIP_VER == 2
struct ip_info {
	ip4_addr_t ip;
	ip4_addr_t netmask;
	ip4_addr_t gw;
};
#endif

typedef struct ip_info ip_info_st;
static_assert(sizeof(ip_info_st) == 12, "ip_info size mismatch");

#endif /* RELIB_IP_INFO_H */

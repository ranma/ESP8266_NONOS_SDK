#ifndef RELIB_IP_ADDR_H
#define RELIB_IP_ADDR_H

#include "lwip/ip_addr.h"

#if LIBLWIP_VER == 2
struct ip_addr {
	uint32_t addr;
};
typedef struct ip4_addr ip_addr_st;

#define IP_ADDR_T const ip4_addr_t
#define IPSTR "%d.%d.%d.%d"
#define IP2STR(ipaddr) ip4_addr1_16(ipaddr), \
    ip4_addr2_16(ipaddr), \
    ip4_addr3_16(ipaddr), \
    ip4_addr4_16(ipaddr)
#else
#define IP_ADDR_T ip_addr_t
typedef struct ip_addr ip_addr_st;
#endif

static_assert(sizeof(ip_addr_st) == 4, "ip_addr size mismatch");

#endif /* RELIB_IP_ADDR_H */

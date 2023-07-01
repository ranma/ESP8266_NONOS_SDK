#ifndef RELIB_IP_ADDR_H
#define RELIB_IP_ADDR_H

struct ip_addr {
	uint32_t addr;
};
typedef struct ip_addr ip_addr_st;

static_assert(sizeof(ip_addr_st) == 4, "ip_addr size mismatch");

#endif /* RELIB_IP_ADDR_H */

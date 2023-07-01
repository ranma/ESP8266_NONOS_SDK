#ifndef RELIB_IF_PARAM_H
#define RELIB_IF_PARAM_H

#include "relib/s/ip_info.h"

struct if_param {
	struct ip_info softap_info;
	struct ip_info sta_info;
	uint8_t softap_mac[6];
	uint8_t sta_mac[6];
};
typedef struct if_param if_param_st;

static_assert(sizeof(if_param_st) == 0x24, "if_param_st size mismatch");

#endif /* RELIB_IF_PARAM_H */

#ifndef RELIB_ESF_MGMT_RAWSBUF_H
#define RELIB_ESF_MGMT_RAWSBUF_H

#include "relib/s/lldesc.h"

struct esf_mgmt_rawsbuf {
	lldesc_st link;
	uint8_t buf[64];
};
static_assert(sizeof(struct esf_mgmt_rawsbuf) == (12 + 64), "esf_mgmt_rawsbuf size mismatch");

typedef struct esf_mgmt_rawsbuf esf_mgmt_rawsbuf_st;

#endif /* RELIB_ESF_MGMT_RAWSBUF_H */

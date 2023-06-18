#ifndef RELIB_ESF_MGMT_RAWLBUF_H
#define RELIB_ESF_MGMT_RAWLBUF_H

#include "relib/s/lldesc.h"

struct esf_mgmt_rawlbuf {
	lldesc_st link;
	uint8_t buf[256];
};
static_assert(sizeof(struct esf_mgmt_rawlbuf) == (12 + 256), "esf_mgmt_rawlbuf size mismatch");

typedef struct esf_mgmt_rawlbuf esf_mgmt_rawlbuf_st;

#endif /* RELIB_ESF_MGMT_RAWLBUF_H */

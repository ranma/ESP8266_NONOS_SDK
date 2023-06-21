#ifndef RELIB_WL_PROFILE_H
#define RELIB_WL_PROFILE_H

#include "relib/s/boot_hdr_param.h"

struct wl_profile {
	/* shallow struct def to avoid pulling in all the other struct members */
	union {
		uint8_t pad1[0x4a4];
		struct {
			boot_hdr_param_st boot_hdr_param;
			uint8_t opmode;
		};
		struct {
			uint8_t pad2[1170];
			uint8_t mac_bakup[6];
		};
	};
} __attribute__((packed));

typedef struct wl_profile wl_profile_st;

static_assert(sizeof(wl_profile_st) == 0x4a4, "wl_profile size mismatch");
static_assert(OFFSET_OF(wl_profile_st, opmode) == 8, "opmode offset mismatch");
static_assert(OFFSET_OF(wl_profile_st, mac_bakup) == 1170, "mac_bakup offset mismatch");

#endif /* RELIB_WL_PROFILE_H */

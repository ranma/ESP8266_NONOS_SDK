#ifndef RELIB_WL_PROFILE_H
#define RELIB_WL_PROFILE_H

#include "relib/s/boot_hdr_param.h"

typedef enum ieee80211_phymode {
	IEEE80211_MODE_AUTO=0,
	IEEE80211_MODE_11B=1,
	IEEE80211_MODE_11G=2,
	IEEE80211_MODE_11NG=3,
	IEEE80211_MODE_11NA=4
} ieee80211_phymode_t;

struct wl_profile {
	/* shallow struct def to avoid pulling in all the other struct members */
	union {
		uint8_t pad1[0x4a4];
		struct {
			boot_hdr_param_st boot_hdr_param;
			uint8_t opmode;
		};
		struct {
			uint8_t pad_836[836];
			ieee80211_phymode_t phyMode;
		};
		struct {
			uint8_t pad_1170[1170];
			uint8_t mac_bakup[6];
		};
	};
} __attribute__((packed));

typedef struct wl_profile wl_profile_st;

static_assert(sizeof(wl_profile_st) == 0x4a4, "wl_profile size mismatch");
static_assert(OFFSET_OF(wl_profile_st, opmode) == 8, "opmode offset mismatch");
static_assert(OFFSET_OF(wl_profile_st, phyMode) == 836, "phyMode offset mismatch");
static_assert(OFFSET_OF(wl_profile_st, mac_bakup) == 1170, "mac_bakup offset mismatch");

#endif /* RELIB_WL_PROFILE_H */

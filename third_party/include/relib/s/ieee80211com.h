#ifndef RELIB_IEEE80211COM_H
#define RELIB_IEEE80211COM_H

#include "relib/s/wl_profile.h"
#include "relib/s/ieee80211_channel.h"
#include "relib/s/ieee80211_rateset.h"
#include "relib/s/wifi_country.h"

struct ieee80211_conn;
struct ieee80211_scaner;
struct ieee80211_key;
struct cnx_mgr;
struct wpa2_funcs;
struct wps_funcs;

typedef struct ieee80211com {
	/* shallow struct def to avoid pulling in all the other struct members */
	union {
		uint8_t pad1[0x6b4];
		struct cnx_mgr *cnxmgr;
		struct {
			uint8_t pad_4[4];
			struct ieee80211_scaner *scaner;
		};
		struct {
			uint8_t pad_16[16];
			union {
				struct {
					struct ieee80211_conn *ic_if0_conn;
					struct ieee80211_conn *ic_if1_conn;
				};
				struct ieee80211_conn* ic_ifx_conn[2];
			};
		};
		struct {
			uint8_t pad_28[28];
			uint32_t ic_flags;
		};
		struct {
			uint8_t pad_44[44];
			ieee80211_rateset_st ic_sup_rates[5];
		};
		struct {
			uint8_t pad_132[132];
			ieee80211_channel_st ic_channels[14];
		};
		struct {
			uint8_t pad_300[300];
			ieee80211_channel_st *ic_home_channel;
			wifi_country_st ic_country;
		};
		struct {
			uint8_t pad_396[396];
			struct ieee80211_key* ic_key[16];
		};
		struct {
			uint8_t pad_474[474];
			uint16_t ic_aid_bitmap;
		};
		struct {
			uint8_t pad_478[478];
			uint8_t ic_mode;
			uint8_t phy_function;
		};
		struct {
			uint8_t pad_484[484];
			struct wpa2_funcs *ic_wpa2;
			struct wps_funcs *ic_wps;
		};
		struct {
			uint8_t pad_500[500];
			uint32_t avs_key_mask;
		};
		struct {
			uint8_t pad_512[512];
			uint8_t ic_bss_num;
			struct ieee80211_bss *ic_ext_ap;
			uint8_t ic_csa_state;
			uint8_t ic_csa_cnt;
			uint8_t ic_csa_chan;
		};
		struct {
			uint8_t pad_524[524];
			wl_profile_st ic_profile;
		};
	};
} ieee80211com_st;

static_assert(sizeof(ieee80211com_st) == 0x6b4, "ieee80211com size mismatch");
static_assert(OFFSET_OF(ieee80211com_st, scaner) == 4, "scaner offset mismatch");
static_assert(OFFSET_OF(ieee80211com_st, ic_ifx_conn) == 16, "ic_ifx_conn offset mismatch");
static_assert(OFFSET_OF(ieee80211com_st, ic_flags) == 28, "ic_flags offset mismatch");
static_assert(OFFSET_OF(ieee80211com_st, ic_mode) == 478, "ic_mode offset mismatch");
static_assert(OFFSET_OF(ieee80211com_st, phy_function) == 479, "phy_function offset mismatch");
static_assert(OFFSET_OF(ieee80211com_st, ic_profile) == 0x20c, "ic_profile offset mismatch");

#endif /* RELIB_IEEE80211COM_H */

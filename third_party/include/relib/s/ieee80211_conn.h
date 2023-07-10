#ifndef RELIB_IEEE80211_CONN_H
#define RELIB_IEEE80211_CONN_H

#include "relib/s/ieee80211_bss.h"

struct netif;
struct hostapd_data;
struct ieee80211_channel;

typedef enum ieee80211_opmode {
	IEEE80211_M_STA=0,
	IEEE80211_M_HOSTAP=1
} ieee80211_opmode_t;

typedef enum ieee80211_state {
	IEEE80211_S_INIT=0,
	IEEE80211_S_SCAN=1,
	IEEE80211_S_AUTH=2,
	IEEE80211_S_ASSOC=3,
	IEEE80211_S_CAC=4,
	IEEE80211_S_RUN=5,
	IEEE80211_S_CSA=6,
	IEEE80211_S_SLEEP=7
} ieee80211_state_t;

struct ieee80211_conn {
	/* shallow struct def to avoid pulling in all the other struct members */
	union {
		struct {
			struct netif *ni_ifp;
			ETSTimer ni_connect_timeout;
			ETSTimer ni_beacon_timeout;
			ETSTimer ni_mgd_probe_ap_timer;
		};
		struct {
			uint8_t pad64[64];
			uint8_t mgd_probe_send_count;
		};
		struct {
			uint8_t pad68[68];
			enum ieee80211_state ni_mlme_state;
		};
		struct {
			uint8_t pad72[72];
			uint8_t ni_macaddr[6];
		};
		struct {
			uint8_t pad80[80];
			uint32_t ni_flags;
		};
		struct {
			uint8_t pad144[144];
			struct ieee80211_bss* ni_bss;
		};
		struct {
			uint8_t pad152[152];
			struct ieee80211_bss* ni_ap_bss[10];
		};
		struct {
			uint8_t pad200[200];
			enum ieee80211_opmode opmode;
			struct hostapd_data *ni_hapd;
			uint8_t ni_connect_step;
			uint8_t ni_connect_err_time;
			uint8_t ni_connect_status;
			uint8_t ni_ap_started:1;
			uint8_t ni_sta_started:1;
			uint8_t ni_pad:6;
			struct ieee80211_channel *ni_channel;
		};
		uint8_t pad672[672];
	};
};

typedef struct ieee80211_conn ieee80211_conn_st;

static_assert(sizeof(ieee80211_conn_st) == 672, "ieee80211_conn size mismatch");
static_assert(OFFSET_OF(ieee80211_conn_st, opmode) == 200, "opmode offset mismatch");

#endif /* RELIB_IEEE80211_CONN_H */

#ifndef RELIB_WL_PROFILE_H
#define RELIB_WL_PROFILE_H

#include "relib/s/boot_hdr_param.h"
#include "relib/s/ieee80211_ssid.h"
#include "relib/s/opmode.h"

#ifdef __USER_INTERFACE_H__
typedef WPS_TYPE_t wps_type_t;
#else
typedef enum wps_type {
    WPS_TYPE_DISABLE=0,
    WPS_TYPE_E_PBC=1,
    WPS_TYPE_E_PIN=2,
    WPS_TYPE_E_DISPLAY=3,
    WPS_TYPE_E_MAX=4,
    WPS_TYPE_R_PBC=5,
    WPS_TYPE_R_PIN=6,
    WPS_TYPE_R_DISPLAY=7,
    WPS_TYPE_R_MAX=8,
    WPS_TYPE_ALL_MAX=9
} wps_type_t;
#endif

typedef enum wps_status {
    WPS_STATUS_DISABLE=0,
    WPS_STATUS_PENDING=1,
    WPS_STATUS_SUCCESS=2,
    WPS_STATUS_MAX=3
} wps_status_t;

struct station_param {
	struct ieee80211_ssid ssid;
	uint8_t dot11_authmode;
	uint8_t authmode;
	uint8_t pairwise_cipher;
	uint8_t pairwise_cipher_len;
	uint8_t group_cipher;
	uint8_t group_cipher_len;
	uint8_t reset_param;
	uint8_t password[65];
	uint8_t pmk[32];
	uint8_t channel;
	uint8_t wepkey[16];
	bool bssid_set;
	uint8_t bssid[6];
};
typedef struct station_param station_param_st;

struct softap_param {
	struct ieee80211_ssid ssid;
	uint8_t password[65];
	uint8_t pmk[32];
	uint8_t channel;
	uint8_t authmode;
	uint8_t ssid_hidden;
	uint8_t max_connection;
};
typedef struct softap_param softap_param_st;

struct wifi_status_led_param {
	bool open_flag;
	uint8_t gpio_id;
	uint8_t status;
};
typedef struct wifi_status_led_param wifi_status_led_param_st;

typedef enum ieee80211_phymode {
	IEEE80211_MODE_AUTO=0,
	IEEE80211_MODE_11B=1,
	IEEE80211_MODE_11G=2,
	IEEE80211_MODE_11NG=3,
	IEEE80211_MODE_11NA=4
} ieee80211_phymode_t;

struct ap_switch_info {
	uint8_t ap_number;
	uint8_t ap_index;
};
typedef struct ap_switch_info ap_switch_info_st;

struct ieee80211_ap_info {
	struct ieee80211_ssid ssid;
	uint8_t password[64];
};
typedef struct ieee80211_ap_info ieee80211_ap_info_st;

struct ieee80211_ap_info_2 {
	bool bssid_set;
	uint8_t bssid[6];
};
typedef struct ieee80211_ap_info_2 ieee80211_ap_info_2_st;

struct ieee80211_ap_info_3 {
	uint8_t pmk[32];
};
typedef struct ieee80211_ap_info_3 ieee80211_ap_info_3_st;

struct wl_profile {
	/* shallow struct def to avoid pulling in all the other struct members */
	union {
		uint8_t pad1[0x4a4];
		struct {
			boot_hdr_param_st boot_hdr_param;
			opmode_t opmode;
		};
		struct {
			uint8_t pad_9[9];
			wifi_status_led_param_st led;
		};
		struct {
			uint8_t pad_12[12];
			station_param_st sta;
		};
		struct {
			uint8_t pad_176[176];
			softap_param_st softap;
		};
		struct {
			uint8_t pad_316[316];
			ap_switch_info_st ap_change_param;
		};
		struct {
			uint8_t pad_320[320];
			ieee80211_ap_info_st save_ap_info[5];
		};
		struct {
			uint8_t pad_820[820];
			uint32_t flags_ht;
			uint16_t htcaps;
			uint16_t htparam;
			uint16_t htextcaps;
		};
		struct {
			uint8_t pad_832[832];
			uint8_t auto_connect;
		};
		struct {
			uint8_t pad_836[836];
			ieee80211_phymode_t phyMode;
		};
		struct {
			uint8_t pad_840[840];
			ieee80211_ap_info_2_st save_ap_info_2[5];
		};
		struct {
			uint8_t pad_876[876];
			uint16_t softap_beacon_interval;
		};
		struct {
			uint8_t pad_880[880];
			wps_type_t wps_type;
		};
		struct {
			uint8_t pad_884[884];
			wps_status_t wps_status;
		};
		struct {
			uint8_t pad_996[996];
			ieee80211_ap_info_3_st pmk_info[5];
		};
		struct {
			uint8_t pad_1170[1170];
			uint8_t mac_bakup[6];
		};
		struct {
			uint8_t pad_1176[1176];
			int8_t minimum_rssi_in_fast_scan;
			uint8_t minimum_auth_mode_in_fast_scan;
			uint8_t sta_open_and_wep_mode_disable;
		};
		struct {
			uint8_t pad_1179[1179];
			uint8_t save_ap_channel_info[5];
			bool all_channel_scan;
		};
	};
} __attribute__((packed));

typedef struct wl_profile wl_profile_st;

static_assert(sizeof(wl_profile_st) == 0x4a4, "wl_profile size mismatch");
static_assert(OFFSET_OF(wl_profile_st, opmode) == 8, "opmode offset mismatch");
static_assert(OFFSET_OF(wl_profile_st, phyMode) == 836, "phyMode offset mismatch");
static_assert(OFFSET_OF(wl_profile_st, mac_bakup) == 1170, "mac_bakup offset mismatch");

#endif /* RELIB_WL_PROFILE_H */

#ifndef RELIB_IEEE80211_SCANER_H
#define RELIB_IEEE80211_SCANER_H

typedef void (*scan_done_cb_t)(void *arg, STATUS status);

struct ieee80211_scaner {
	uint32_t ss_type;
	ETSTimer ss_inter_channel_timer;
	ETSTimer ss_enter_oper_channel_timer;
	uint32_t ss_inter_channel_interval;
	uint32_t ss_maxact_duration;
	uint32_t ss_minact_duration;
	uint32_t ss_pas_duration;
	scan_done_cb_t ss_cb;
	void * ss_cb_arg;
	uint8_t priority;
	uint8_t ss_channel_index;
	uint8_t ss_state;
	uint8_t ss_just_scan;
	struct ieee80211_ssid ss_probe_table[2];
	uint8_t ss_probe_flag[2];
	uint8_t ss_probe_index;
	uint8_t ss_channel_hint;
	uint8_t ss_bssid[6];
	uint8_t ss_bssid_set;
	uint8_t ss_show_hidden;
};
typedef struct ieee80211_scaner ieee80211_scaner_st;
static_assert(sizeof(ieee80211_scaner_st) == 156, "ieee80211_scaner_st size mismatch");

#endif /* RELIB_IEEE80211_SCANER_H */

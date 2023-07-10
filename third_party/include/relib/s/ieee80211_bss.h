#ifndef RELIB_IEEE80211_BSS_H
#define RELIB_IEEE80211_BSS_H

#include "ets_sys.h"
#include "relib/s/wifi_country.h"
#include "relib/s/ieee80211_rateset.h"

struct ieee80211_channel;

struct wmeParams {
	uint8_t wmep_acm;
	uint8_t wmep_aifsn;
	uint8_t wmep_logcwmin;
	uint8_t wmep_logcwmax;
	uint8_t wmep_txopLimit;
	uint8_t wmep_noackPolicy;
};

struct chanAccParams {
	uint8_t cap_info;
	struct wmeParams cap_wmeParams[4];
};

struct rc_metrics {
	int8_t rssi_avg;
	int8_t rssi_dt;
	int8_t rssi_last;
	int8_t cs_utility;
	int8_t roam_bias;
	uint8_t bss_age;
	bool pmkid_valid;
};

typedef uint16_t ieee80211_seq_t;

struct esf_buf;

struct ieee80211_psq_head {
	struct esf_buf *head;
	struct esf_buf *tail;
	int len;
};

struct ieee80211_psq {
	int psq_len;
	int psq_maxlen;
	int psq_drops;
	struct ieee80211_psq_head psq_head[2];
};


struct ieee80211_bss {
	uint8_t ni_bssid[6];
	uint8_t bss_flags;
	uint32_t ni_flags;
	bool ni_uapsd;
	union {
		uint8_t data[8];
		uint64_t tsf;
	} ni_tstamp;
	uint8_t ni_datapolicy;
	uint16_t ni_associd;
	uint16_t ni_intval;
	uint16_t ni_capinfo;
	uint16_t ni_erp;
	uint8_t ni_dtim_period;
	uint8_t ni_dtim_count;
	uint8_t ni_wpa_ie_len;
	uint8_t bss_phy_mode;
	uint8_t bss_op_phy_mode;
	uint8_t ni_wpa_ie[64];
	struct ieee80211_rateset ni_rates;
	struct chanAccParams ni_chan_ac_params;
	struct rc_metrics ni_rc_metrics;
	struct ieee80211_channel * ni_channel;
	ieee80211_seq_t ni_txseqs[17];
	ieee80211_seq_t ni_rxseqs[17];
	struct ieee80211_psq ni_psq;
	struct wpa_state_machine * ni_wpa_sm;
	uint8_t ni_ptk_index;
	uint8_t ni_gtk_index;
	uint8_t ni_last_rev_auth;
	uint8_t ni_gtk_index_array[4];
	ETSTimer ni_inactivity_timer;
	uint32_t ni_last_rev_time;
	uint32_t ni_ip;
	struct wifi_country ni_country;
};
typedef struct ieee80211_bss ieee80211_bss_st;

static_assert(sizeof(ieee80211_bss_st) == 312, "ieee80211_bss_st size mismatch");

#endif /* RELIB_IEEE80211_BSS_H */

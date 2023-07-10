#ifndef RELIB_IEEE80211_SCANPARAMS_H
#define RELIB_IEEE80211_SCANPARAMS_H

struct ieee80211_scanparams {
	uint8_t * bssid;
	uint8_t chan;
	uint8_t bchan;
	uint16_t capinfo;
	uint16_t erp;
	uint16_t bintval;
	uint8_t timoff;
	uint8_t * ies;
	size_t ies_len;
	uint8_t * tim;
	uint8_t * tstamp;
	uint8_t * country;
	uint8_t * ssid;
	uint8_t * rates;
	uint8_t * xrates;
	uint8_t * doth;
	uint8_t * wpa;
	uint8_t * rsn;
	uint8_t * wme;
	uint8_t * htcap;
	uint8_t * htinfo;
	uint8_t * csa;
	uint8_t * wps;
	uint8_t * esp_mesh_ie;
	uint8_t simple_pair;
	uint16_t freqcal_val;
};
typedef struct ieee80211_scanparams ieee80211_scanparams_st;
static_assert(sizeof(ieee80211_scanparams_st) == 88, "ieee80211_scanparams_st size mismatch");

#endif /* RELIB_IEEE80211_SCANPARAMS_H */

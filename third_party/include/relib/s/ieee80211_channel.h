#ifndef RELIB_IEEE80211_CHANNEL_H
#define RELIB_IEEE80211_CHANNEL_H

struct ieee80211_channel {
	uint32_t ch_flags;
	uint16_t ch_freq;
	uint8_t ch_ieee;
	int8_t ch_maxpower;
	uint8_t ch_attr;
	int8_t ch_maxregpower;
};
typedef struct ieee80211_channel ieee80211_channel_st;

static_assert(sizeof(ieee80211_channel_st) == 12, "ieee80211_channel_st size mismatch");

#endif /* RELIB_IEEE80211_CHANNEL_H */

#ifndef RELIB_IEEE80211_RATESET_H
#define RELIB_IEEE80211_RATESET_H

struct ieee80211_rateset {
	uint8_t rs_nrates;
	uint8_t rs_rates[15];
};
typedef struct ieee80211_rateset ieee80211_rateset_st;
static_assert(sizeof(ieee80211_rateset_st) == 16, "ieee80211_rateset_st size mismatch");

#endif /* RELIB_IEEE80211_RATESET_H */

#ifndef RELIB_IEEE80211_FRAME_H
#define RELIB_IEEE80211_FRAME_H

struct ieee80211_frame {
	uint8_t i_fc[2];
	uint8_t i_dur[2];
	uint8_t i_addr1[6];
	uint8_t i_addr2[6];
	uint8_t i_addr3[6];
	uint8_t i_seq[2];
};
typedef struct ieee80211_frame ieee80211_frame_st;
static_assert(sizeof(ieee80211_frame_st) == 24, "ieee80211_frame_st size mismatch");

struct ieee80211_qosframe_addr4 {
	uint8_t i_fc[2];
	uint8_t i_dur[2];
	uint8_t i_addr1[6];
	uint8_t i_addr2[6];
	uint8_t i_addr3[6];
	uint8_t i_seq[2];
	uint8_t i_addr4[6];
	uint8_t i_qos[2];
};
typedef struct ieee80211_qosframe_addr4 ieee80211_qosframe_addr4_st;
static_assert(sizeof(ieee80211_qosframe_addr4_st) == 32, "ieee80211_qosframe_addr4_st size mismatch");


#endif /* RELIB_IEEE80211_FRAME_H */

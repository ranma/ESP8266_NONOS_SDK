#ifndef RELIB_ESF_RX_DESC_H
#define RELIB_ESF_RX_DESC_H

struct ieee80211_channel;

struct esf_rx_desc {
	uint32_t flags:12;
	uint32_t antenna:4;
	uint32_t service:8;
	uint32_t rsv:8;
	uint32_t timestamp:32;
	struct ieee80211_channel *channel;
};

typedef struct esf_rx_desc esf_rx_desc_st;

#endif /* RELIB_ESF_RX_DESC_H */

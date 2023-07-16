#ifndef RELIB_ESF_RX_DESC_H
#define RELIB_ESF_RX_DESC_H

struct ieee80211_channel;

struct esf_rx_desc {
	uint32_t flags:12;
	uint32_t antenna:4;
	uint8_t  service;
	uint8_t  rsv;
	uint32_t timestamp;
	struct ieee80211_channel *channel;
};

typedef struct esf_rx_desc esf_rx_desc_st;

#endif /* RELIB_ESF_RX_DESC_H */

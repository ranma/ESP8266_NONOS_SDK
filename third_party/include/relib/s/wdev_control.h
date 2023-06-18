#ifndef RELIB_WDEV_CONTROL_H
#define RELIB_WDEV_CONTROL_H

#include "relib/s/lldesc.h"

struct wdev_control;

struct wdev_control {
	uint16_t rx_remain_blocks;
	uint16_t rx_remain_blocks2;
	uint8_t rx_ampdu_len_remains;
	uint8_t sniffer_mode;
	lldesc_st *rx_head;
	lldesc_st *rx_tail;
	lldesc_st *rx_head2;
	lldesc_st *rx_tail2;
	lldesc_st fake_tail;
	uint8_t fake_buf[4];
	lldesc_st *tx_head;
	lldesc_st *tx_tail;
	lldesc_st *rx_ampdu_len_head;
	lldesc_st *rx_ampdu_len_tail;
	uint32_t bssid_key_mask[2];
};

typedef struct wdev_control wdev_control_st;

#endif /* RELIB_WDEV_CONTROL_H */

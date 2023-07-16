#ifndef RELIB_RXCONTROL_H
#define RELIB_RXCONTROL_H

enum rxend_state {
	RX_OK=0,
	// From esp_wmac.h
	RX_PYH_ERR_MIN=0x42,
	RX_AGC_ERR_MIN=0x42,
	RX_AGC_ERR_MAX=0x47,
	RX_OFDM_ERR_MIN=0x50,
	RX_OFDM_ERR_MAX=0x58,
	RX_CCK_ERR_MIN=0x59,
	RX_CCK_ERR_MAX=0x5F,
	RX_ABORT=0x80,
	RX_SF_ERR=0x40,
	RX_FCS_ERR=0x41,
	RX_AHBOV_ERR=0xC0,
	RX_BUFOV_ERR=0xC1,
	RX_BUFINV_ERR=0xC2,
	RX_AMPDUSF_ERR=0xC3,
	RX_AMPDUBUFOV_ERR=0xC4,
	RX_MACBBFIFOOV_ERR=0xC5,
	RX_RPBM_ERR=0xC6,
	RX_BTFORCE_ERR=0xC7,
	RX_SECOV_ERR=0xE1,
	RX_SECPROT_ERR0=0xE2,
	RX_SECPROT_ERR1=0xE3,
	RX_SECKEY_ERR=0xE4,
	RX_SECCRLEN_ERR=0xE5,
	RX_SECFIFO_TIMEOUT=0xE6,
	RX_WEPICV_ERR=0xF0,
	RX_TKIPICV_ERR=0xF4,
	RX_TKIPMIC_ERR=0xF5,
	RX_CCMPMIC_ERR=0xF8,
	RX_WAPIMIC_ERR=0xFC,
} __attribute__((packed));
typedef enum rxend_state rxend_state_t;

struct rx_control {
	// 0x00
	int rssi:8;
	uint32_t rate:4;
	uint32_t is_group:1;
	int pad1:1;
	uint32_t sig_mode:2;
	// 0x02
	uint32_t legacy_length:12;
	uint32_t damatch0:1;
	uint32_t damatch1:1;
	uint32_t bssidmatch0:1;
	uint32_t bssidmatch1:1;
	// 0x04
	uint32_t MCS:7;
	uint32_t CWB:1;
	uint32_t HT_length:16;
	uint32_t Smoothing:1;
	uint32_t Not_Sounding:1;
	int pad2:1;
	uint32_t Aggregation:1;
	uint32_t STBC:2;
	uint32_t FEC_CODING:1;
	uint32_t SGI:1;
	// 0x08
	rxend_state_t rxend_state;
	uint32_t ampdu_cnt:8;
	// 0x0a
	uint32_t channel:4;
	int pad3:4;
	int noise_floor:8;
};
typedef struct rx_control rx_control_st;
static_assert(sizeof(rx_control_st) == 12, "rx_control_st size mismatch");

#endif /* RELIB_RXCONTROL_H */

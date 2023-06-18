#ifndef RELIB_ESF_TX_DESC_H
#define RELIB_ESF_TX_DESC_H

struct esf_rc_sched;

struct esf_tx_desc {
	uint32_t p2p:1;
	uint32_t ifidx:1;
	uint32_t qid:4;
	uint32_t flags:26;
	uint32_t tid:4;
	uint32_t rrc:4;
	uint32_t src:6;
	uint32_t lrc:6;
	uint32_t ac:3;
	uint32_t acktime:9;
	uint32_t rate:8;
	uint32_t ackrssi:8;
	uint32_t ctstime:16;
	uint32_t kid:8;
	uint32_t crypto_type:4;
	uint32_t antenna:4;
	uint32_t reserved:8;
	uint32_t status:8;
	uint32_t comp_cb_map:32;
	uint32_t data_flags:32;
	uint32_t timestamp:32;
	struct esf_rc_sched *rcSched;
};

typedef struct esf_tx_desc esf_tx_desc_st;

#endif /* RELIB_ESF_TX_DESC_H */

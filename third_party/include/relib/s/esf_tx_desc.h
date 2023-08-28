#ifndef RELIB_ESF_TX_DESC_H
#define RELIB_ESF_TX_DESC_H

/* Maybe: Additional descs in this MSDU */
#define ESF_TX_FLAG_MSDU_MORE       (1 <<  6)
/* Maybe: Last desc in this MSDU */
#define ESF_TX_FLAG_MSDU_END        (1 <<  7)
#define ESF_TX_FLAG_BUF_TX_PB       (1 << 13)
#define ESF_TX_FLAG_BUF_MGMT_LBUF   (1 << 14)
#define ESF_TX_FLAG_BUF_MGMT_SBUF   (1 << 15)
#define ESF_TX_FLAG_BUF_BAR         (1 << 21)
#define ESF_TX_FLAG_BUF_MGMT_LLBUF  (1 << 24)

enum tx_status {
	TX_DESCRIPTOR_STATUS_NEW=0,
	TX_DESCRIPTOR_STATUS_SUCCESS=1,
	TX_DESCRIPTOR_STATUS_SRC_EXCEED=2,
	TX_DESCRIPTOR_STATUS_LRC_EXCEED=3,
	TX_DESCRIPTOR_STATUS_DISCARD=4,
	TX_DESCRIPTOR_STATUS_AP_RPT=5
} __attribute__((packed));
typedef enum tx_status tx_status_t;

struct esf_rc_sched;

struct esf_tx_desc {
	uint32_t p2p:1;
	uint32_t ifidx:1;
	uint32_t qid:4;  /* Queue ID */
	uint32_t flags:26;

	uint32_t tid:4;
	uint32_t rrc:4;
	uint32_t src:6;  /* short retry count */
	uint32_t lrc:6;  /* long retry count */
	uint32_t ac:3;   /* access category */
	uint32_t acktime:9;

	uint32_t rate:8;
	uint32_t ackrssi:8;
	uint32_t ctstime:16;

	uint32_t kid:8;
	uint32_t crypto_type:4;
	uint32_t antenna:4;
	uint32_t reserved:8;
	tx_status_t status:8;

	uint32_t comp_cb_map:32;  /* enabled completion callbacks bitmap */
	uint32_t data_flags:32;
	uint32_t timestamp:32;
	struct esf_rc_sched *rcSched;
};

typedef struct esf_tx_desc esf_tx_desc_st;
static_assert(sizeof(esf_tx_desc_st) == 32, "esf_tx_desc size mismatch");

#endif /* RELIB_ESF_TX_DESC_H */

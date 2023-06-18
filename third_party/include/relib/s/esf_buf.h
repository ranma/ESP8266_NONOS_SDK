#ifndef RELIB_ESF_BUF_H
#define RELIB_ESF_BUF_H

struct esf_rx_desc;
struct esf_tx_desc;
struct lldesc;
struct pbuf;
struct target_rc;

struct esf_buf {
	struct pbuf *pbuf;
	struct lldesc *ds_head;
	struct lldesc *ds_tail;
	uint16_t ds_len;
	uint8_t * buf_begin;
	uint16_t hdr_len;
	uint16_t data_len;
	int16_t chl_freq_offset;
	struct target_rc *trc;
	union {
		struct {
			struct esf_buf *stqe_next;
		} bqentry;
		struct esf_buf *stqe_next;
	};
	union {
		union {
			struct esf_tx_desc *tx_desc;
			struct esf_rx_desc *rx_desc;
		} desc;
		struct esf_tx_desc *tx_desc;
		struct esf_rx_desc *rx_desc;
	};
};
static_assert(sizeof(struct esf_buf) == 40, "esf_buf size mismatch");

enum esf_buf_type {
	ESF_BUF_TX_PB=1,
	ESF_BUF_TX_SIP=2,
	ESF_BUF_TX_RAW=3,
	ESF_BUF_MGMT_LBUF=4,
	ESF_BUF_MGMT_SBUF=5,
	ESF_BUF_MGMT_LLBUF=6,
	ESF_BUF_BAR=7,
	ESF_BUF_RX_BLOCK=8,
};

typedef struct esf_buf esf_buf_st;
typedef enum esf_buf_type esf_buf_type_t;

#endif /* RELIB_ESF_BUF_H */

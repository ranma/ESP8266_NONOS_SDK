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
	struct esf_buf *stqe_next;
	union {
		struct esf_tx_desc *tx_desc;
		struct esf_rx_desc *rx_desc;
	};
};

typedef struct esf_buf esf_buf_st;

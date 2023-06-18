#ifndef RELIB_ESF_BUF_CTX_H
#define RELIB_ESF_BUF_CTX_H

struct esf_buf;

struct esf_buf_ctx {
	struct esf_buf *eb_tx_free_list;
	struct esf_buf *eb_mgmt_l_free_list;
	struct esf_buf *eb_mgmt_s_free_list;
	struct esf_buf *eb_tx_bar_free_list;
	struct esf_buf *eb_rx_block_free_list;
};

typedef struct esf_buf_ctx esf_buf_ctx_st;

#endif /* RELIB_ESF_BUF_CTX_H */

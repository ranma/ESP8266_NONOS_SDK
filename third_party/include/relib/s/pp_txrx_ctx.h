#ifndef RELIB_PP_TXRX_CTX_H
#define RELIB_PP_TXRX_CTX_H

#include "relib/queue.h"
#include "relib/s/pp_txq.h"

struct esf_buf;

typedef void (*pp_tx_cb_t)(struct esf_buf *eb);

struct pp_txrx_ctx {
	uint32_t queue_enable_mask;
	uint32_t queue_enable_ac_mask[4];
	uint8_t queue_select[4];
	pp_txq_st txq[8];
	STAILQ_HEAD(, esf_buf) wait_txq[2];
	STAILQ_HEAD(, esf_buf) done_txq;
	STAILQ_HEAD(, esf_buf) rxq;
	uint32_t txCBmap;
	pp_tx_cb_t txCBTab[10];
	uint32_t sent_beacon;
	uint32_t stuck_beacon;
};

typedef struct pp_txrx_ctx pp_txrx_ctx_st;

static_assert(OFFSET_OF(pp_txrx_ctx_st, txCBTab) == 0x13c, "txCBTab offset mismatch");

#endif /* RELIB_PP_TXRX_CTX_H */

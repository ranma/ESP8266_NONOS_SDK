#ifndef RELIB_PP_TXQ_H
#define RELIB_PP_TXQ_H

#include "relib/queue.h"

struct esf_buf;

struct pp_txq {
	STAILQ_HEAD(, esf_buf) queue;
	uint8_t bar_rrc;
	uint8_t ap_paused;
	uint16_t ssn;
	uint8_t tid;
	uint8_t conn_idx;
	uint8_t ifidx;
	bool nonqos;
	bool active;
	struct esf_buf *ap_eb;
	uint32_t bitmap_high;
	uint32_t bitmap_low;
};

typedef struct pp_txq pp_txq_st;

#endif /* RELIB_PP_TXQ_H */

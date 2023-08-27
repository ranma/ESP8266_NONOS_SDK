#ifndef RELIB_ACCESS_H
#define RELIB_ACCESS_H

struct esf_buf;

struct access {
	struct esf_buf *tx_frame;
	uint8_t ac;
	uint8_t aifs;
	uint8_t cw;
	uint8_t cw_min;
	uint8_t cw_max;
	uint8_t qsrc;
	uint8_t qlrc;
	bool intxop;
	bool txop_enough;
	bool trytxop;
	bool to_clear;
	bool acm;
	uint8_t org_acktype;
	uint8_t state;
	uint8_t end_status;
	int16_t txop;
	int16_t txop_delta;
	uint16_t txop_max;
	uint16_t MSDULifetime;
	uint32_t success_count;
	uint32_t failure_count;
};

static_assert(sizeof(struct access) == 36, "access size mismatch");

typedef struct access access_st;

#endif /* RELIB_ACCESS_H */

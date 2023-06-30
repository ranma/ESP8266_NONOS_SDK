#ifndef RELIB_RST_INFO_H
#define RELIB_RST_INFO_H

enum rst_reason {
	REASON_DEFAULT_RST,
	REASON_WDT_RST,
	REASON_EXCEPTION_RST,
	REASON_SOFT_WDT_RST,
	REASON_SOFT_RESTART,
	REASON_DEEP_SLEEP_AWAKE,
	REASON_EXT_SYS_RST,
};
typedef enum rst_reason rst_reason_t;

struct rst_info {
	rst_reason_t reason;
	uint32_t exccause;
	uint32_t epc1;
	uint32_t epc2;
	uint32_t epc3;
	uint32_t excvaddr;
	uint32_t depc;
};
typedef struct rst_info rst_info_st;
static_assert(sizeof(rst_info_st) == 0x1c, "rst_info struct size mismatch");

#endif /* RELIB_RST_INFO_H */

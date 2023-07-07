#ifndef RELIB_CHM_CTX_H
#define RELIB_CHM_CTX_H

#include "osapi.h"

struct ieee80211com;
struct ieee80211_channel;

typedef void (*chm_cb_t)(void *arg, STATUS status);
typedef uint8_t chm_state_t;

struct chm_op {
	struct ieee80211_channel *channel;
	uint32 min_duration;
	uint32 max_duration;
	void * arg;
	chm_cb_t start_fn;
	chm_cb_t end_cb;
};
typedef struct chm_op chm_op_st;

static_assert(sizeof(chm_op_st) == 24, "chm_op_st size mismatch");

struct chm_ctx {
	struct ieee80211com *ic;
	chm_op_st op;
	chm_cb_t op_cb;
	void * op_cb_arg;
	ETSTimer chm_min_dwell_timer;
	ETSTimer chm_max_dwell_timer;
	struct ieee80211_channel *current_channel;
	bool op_lock;
	uint8_t op_priority;
	chm_state_t state;
};
typedef struct chm_ctx chm_ctx_st;

static_assert(sizeof(chm_ctx_st) == 84, "chm_ctx_st size mismatch");

#endif /* RELIB_CHM_CTX_H */

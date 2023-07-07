#ifndef RELIB_CNX_MGR_H
#define RELIB_CNX_MGR_H

#include "relib/s/ieee80211_bss.h"

typedef enum cnx_state {
	CNX_S_CONNECTED=1,
	CNX_S_DISCONNECTED=2,
	CNX_S_AT_HOME=4,
	CNX_S_OFF_HOME=8,
	CNX_S_ROAMING=16
} cnx_state_t;

typedef enum cnx_connect_type {
	CONNECT_WITHOUT_SCAN=1,
	CONNECT_ASSOC_POLICY_USER=2,
	CONNECT_SEND_REASSOC=4
} cnx_connect_type_t;

struct cnx_mgr {
	cnx_state_t cnx_state;
	cnx_connect_type_t cnx_type;
	struct ieee80211_bss cnx_bss_table[3];
	struct ieee80211_bss *cnx_rc_list[3];
	uint8_t cnx_rc_weight;
	uint8_t cnx_rc_list_size;
	uint8_t cnx_rc_index;
	struct ieee80211_bss *cnx_cur_bss;
	struct ieee80211_bss *cnx_last_bss;
	uint32_t cnx_roam_start;
	uint32_t cnx_roam_trigger;
	uint8_t cnx_authMode;
};
typedef struct cnx_mgr cnx_mgr_st;

static_assert(sizeof(cnx_mgr_st) == 984, "cnx_mgr_st size mismatch");

#endif /* RELIB_CNX_MGR_H */

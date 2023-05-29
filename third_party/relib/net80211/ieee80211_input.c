#define MEM_DEFAULT_USE_DRAM 1

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "c_types.h"
#include "ets_sys.h"
#include "mem.h"
#include "lwip/pbuf.h"
#include "lwip/netif.h"
#include "relib/ets_rom.h"
#include "relib/s/esf_buf.h"
#include "relib/s/ieee80211_conn.h"
#include "relib/s/lldesc.h"

int ppRecycleRxPkt(esf_buf_st *eb); /* system_pp_recycle_rx_pkt is an alias/trampoline */

void ICACHE_FLASH_ATTR
ieee80211_deliver_data(ieee80211_conn_st *conn, esf_buf_st *eb)
{
	if (eb == NULL) {
		return;
	}

	if ((conn->ni_ifp->flags & NETIF_FLAG_LINK_UP) == 0) {
		ppRecycleRxPkt(eb);
	}
	else {
		struct pbuf *p = pbuf_alloc(PBUF_RAW,eb->data_len,PBUF_REF);
		p->payload = eb->ds_head->buf;
		eb->pbuf = p;
		p->eb = eb;
		ets_post(PRIO_IF0_STA,0,(ETSParam)p);
		ets_printf("ieee80211_deliver_data!\n");
	}
}

void
jmp_ieee80211_deliver_data(ieee80211_conn_st *conn, esf_buf_st *eb)
{
	ieee80211_deliver_data(conn, eb);
}

/*
         U ccmp
         U ets_bzero
         U ets_memcmp
         U ets_memcpy
         U ets_memset
         U ets_post
         U g_ic
         U ieee80211_chan2ieee
         U max_11n_rate
         U pbuf_alloc
         U ppRecycleRxPkt
         U pvPortMalloc
         U rc_set_rate_limit_id
00000000 T ieee80211_is_support_rate
00000000 T ieee80211_setup_rateset
00000000 B max_11b_rate
00000000 t mem_debug_file_107
00000000 D rate_11b_rate2_rateid_table
00000004 T check_max_11bg_rate
00000004 t ieee80211_is_11b_rate
00000004 B max_11g_rate
00000008 T clean_rate_set
00000008 T ieee80211_alloc_challenge
00000008 T ieee80211_parse_action
00000008 T ieee80211_setup_phy_mode
00000008 B rate_number
0000000c T ieee80211_deliver_data
0000000c t rsn_keymgmt
0000000c t wpa_keymgmt
00000010 T set_max_fixed_rate
00000014 T ieee80211_parse_rsn
00000014 T ieee80211_parse_wpa
00000014 t wpa_cipher
00000018 T ieee80211_setup_rates
00000018 t rsn_cipher
00000020 D rate_11g_rate2_rateid_table
0000002c T ieee80211_decap
00000038 T set_rate_limit
0000003c T ieee80211_parse_beacon
00000080 D rate_11n_rate2_rateid_table
000000e0 D rate2_phyrate_table
*/

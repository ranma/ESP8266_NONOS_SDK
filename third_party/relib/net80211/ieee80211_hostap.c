#define MEM_DEFAULT_USE_DRAM 1

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "c_types.h"
#include "ets_sys.h"
#include "mem.h"
#include "lwip/pbuf.h"
#include "relib/ets_rom.h"
#include "relib/s/lldesc.h"
#include "relib/s/esf_buf.h"

void
jmp_hostap_deliver_data(void *unused_param1, esf_buf_st *eb)
{
	hostap_deliver_data(unused_param1, eb);
}

void ICACHE_FLASH_ATTR
hostap_deliver_data(void *unused_param1, esf_buf_st *eb)
{
	if (eb == NULL) {
		return;
	}

	struct pbuf *p = pbuf_alloc(PBUF_RAW,eb->data_len,PBUF_REF);
	p->payload = eb->ds_head->buf;
	eb->pbuf = p;
	p->eb = eb;
	ets_post(PRIO_IF1_AP,0,(ETSParam)p);
	ets_printf("hostap_deliver_data!\n");
}

/*
         U AllFreqOffsetInOneChk
         U AvgFreqOffsetInOneChk
         U chip_v6_set_chan_offset
         U chm_check_same_channel
         U chm_get_current_channel
         U chm_set_current_channel
         U cnx_node_alloc
         U cnx_node_join
         U cnx_node_leave
         U cnx_node_search
         U __divsi3
         U eagle_lwip_if_alloc
         U eagle_lwip_if_free
         U esf_buf_recycle
         U ethbroadcast
         U ets_delay_us
         U ets_intr_lock
         U ets_intr_unlock
         U ets_memcmp
         U ets_memcpy
         U ets_post
         U ets_strlen
         U ets_timer_arm_new
         U ets_timer_disarm
         U ets_timer_setfn
         U event_cb
         U fpm_close
         U fpm_do_sleep
         U fpm_is_open
         U fpm_open
         U g_ic
         U gpio_output_set
         U HdlAllBuffedEb
         U hostapd_setup_wpa_psk
         U ic_bss_info_update
         U ic_set_vif
         U ieee80211_beacon_alloc
         U ieee80211_chan2ieee
         U ieee80211_crypto_decap
         U ieee80211_decap
         U ieee80211_mesh_quick_get
         U ieee80211_node_pwrsave
         U ieee80211_output_pbuf
         U ieee80211_parse_beacon
         U ieee80211_psq_init
         U ieee80211_recv_action
         U ieee80211_rfid_locp_recv
         U ieee80211_send_deauth
         U ieee80211_send_mgmt
         U ieee80211_send_nulldata
         U ieee80211_send_proberesp
         U ieee80211_set_tim
         U ieee80211_setup_rates
         U info
         U netif_set_default
         U netif_set_down
         U netif_set_up
         U noise_check_loop
         U NoiseTimerInterval
         U os_printf_plus
         U pbuf_alloc
         U pbuf_free
         U periodic_cal_top
         U PktsNumInOneChk
         U pp_disable_noise_timer
         U ppRecycleRxPkt
         U ppRegisterTxCallback
         U ppTxPkt
         U pvPortZalloc
         U pwrsave_flushq
         U resend_eapol
         U reset_noise_timer
         U scan_start
         U test_freq_val_first
         U test_freq_val_force_flag
         U TestStaFreqCalValDev
         U TestStaFreqCalValFilter
         U TestStaFreqCalValInput
         U TestStaFreqCalValOK
         U timer2_ms_flag
         U total_buffed_eb_num
         U __udivsi3
         U __umodsi3
         U vPortFree
         U wDevDisableRx
         U wDev_DisableUcRx
         U wDev_EnableUcRx
         U wDev_Get_Next_TBTT
         U wDev_remove_KeyEntry_all_cnx
         U wDev_Reset_TBTT
         U wDev_Set_Beacon_Int
         U wDev_SetRxPolicy
         U wifi_get_macaddr
         U wifi_get_opmode
         U wpa_init
         U wpa_receive
00000000 B BcnEb_update
00000000 t ieee80211_hdrsize
00000000 t mem_debug_file_127
00000000 D TmpSTAAPCloseAP
00000004 b BcnEb_86
00000004 r brates$8460_117_3
00000004 T freqcal_scan_done
00000004 t hostap_recv_ctl
00000004 t is11bclient
00000008 b bcn_freqcal_start_addr_89
00000008 t hostap_deliver_data
0000000c b tim_offset_90
0000000d B BcnWithMcastSendStart
0000000e B BcnWithMcastSendCnt
0000000f b beacon_send_start_flag_108
00000010 b beacon_timer_112
00000020 t flash_str$8964_127_5
00000024 b BcnIntvl_114
00000024 t hostap_recv_pspoll
00000028 B PendFreeBcnEb
00000029 b APBcnRecv_182
0000002a B ap_freq_force_to_scan
0000002c B ApFreqCalTimer
0000002c t ieee80211_hostap_deinit
00000030 t hostap_auth_open
00000040 B APRecvBcnStartTick
00000040 t flash_str$8974_127_6
00000040 T ieee80211_hostap_attach
00000044 T hostap_handle_timer
00000044 b period_cal_cnt$7743_71_1
00000048 b twice_no_bcn$8961_127_2
0000004c t ieee80211_hostap_init
00000060 t ieee80211_hostapd_beacon_txcb
00000094 T hostap_input
00000094 T wifi_softap_start
000000a0 T wifi_softap_stop
000000a4 t hostap_recv_mgmt
000000b4 t ieee80211_hostap_send_beacon
00000144 t ApFreqCalTimerCB
*/

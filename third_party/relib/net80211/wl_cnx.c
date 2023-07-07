#include <assert.h>
#include <stdint.h>

#include "c_types.h"
#include "mem.h"
#include "osapi.h"
#include "spi_flash.h"
#include "gpio.h"

#include "relib/relib.h"
#include "relib/esp8266.h"
#include "relib/xtensa.h"
#include "relib/ets_rom.h"

#include "relib/s/ieee80211com.h"
#include "relib/s/ieee80211_conn.h"
#include "relib/s/wl_profile.h"
#include "relib/s/cnx_mgr.h"

#if 1
#define DPRINTF(fmt, ...) do { \
	ets_printf("[@%p] " fmt, __builtin_return_address(0), ##__VA_ARGS__); \
	while ((UART0->STATUS & 0xff0000) != 0); \
} while (0)
#else
#define DPRINTF(fmt, ...)
#endif

#define g_cnxMgr g_cnxMgr_84

extern ieee80211com_st g_ic; /* ieee80211.c */
extern ETSTimer sta_con_timer; /* wl_cnx.c */
extern cnx_mgr_st g_cnxMgr; /* wl_cnx.c:84 */
extern uint8_t backup_ni_connect_status; /* wl_cnx.c */

typedef enum bcn_policy {
	RX_DISABLE=0,
	RX_ANY=1,
	RX_BSSID=2,
} bcn_policy_t;

typedef void (*scan_done_cb_t)(void *arg, STATUS status);
typedef void (*cnx_traverse_rc_list_cb_t)(void *arg, STATUS status);

void wDev_SetRxPolicy(bcn_policy_t policy, uint8_t index, uint8_t *bssid);
void scan_remove_bssid(void);
void scan_set_desChan(uint8_t channel);
void scan_add_probe_ssid(uint8_t idx, uint8_t *ssid, uint8_t ssid_len, bool flag);
void scan_hidden_ssid(uint8_t set);
uint8_t scan_prefer_chan(uint8_t *ssid);
uint8_t scan_build_chan_list(uint32_t scan_type, uint8_t prefer_chan);
int scan_start(uint32_t type, uint8_t priority, scan_done_cb_t cb, void *arg);
uint8_t ieee80211_regdomain_min_chan(void);
uint8_t ieee80211_regdomain_max_chan(void);

void ICACHE_FLASH_ATTR
cnx_sta_connect_led_timer_cb(void *unused_arg)
{
	int pin = g_ic.ic_profile.led.gpio_id & 0x1f;
	gpio_output_set((uint32_t)g_ic.ic_profile.led.status << pin,
			(uint32_t)((g_ic.ic_profile.led.status & 1) == 0) << pin,
			1 << pin, 0);
	g_ic.ic_profile.led.status = !g_ic.ic_profile.led.status;
}

STATUS cnx_traverse_rc_list(cnx_traverse_rc_list_cb_t cb, void *arg);
void cnx_do_handoff(void);

void ICACHE_FLASH_ATTR
cnx_start_handoff_cb(void *arg,STATUS status)
{
	DPRINTF("cnx_start_handoff_cb(%p, %d)\n", arg, status);
	if (status == OK) {
		cnx_do_handoff();
	}
}

void ICACHE_FLASH_ATTR
cnx_start_connect(uint32 scan_type)
{
	DPRINTF("cnx_start_connect(0x%08x)\n", scan_type);
	backup_ni_connect_status = (g_ic.ic_if0_conn)->ni_connect_status;
	if ((g_ic.ic_if0_conn)->ni_connect_err_time == 0) {
		(g_ic.ic_if0_conn)->ni_connect_status = 1;
	}
	(g_ic.ic_if0_conn)->ni_connect_step = 1;
	(g_ic.ic_if0_conn)->ni_flags = (g_ic.ic_if0_conn)->ni_flags & 0xffffffbf;
	g_cnxMgr.cnx_state = g_cnxMgr.cnx_state | CNX_S_ROAMING;
	scan_hidden_ssid(0);
	if (scan_type == 0) {
		cnx_traverse_rc_list(cnx_start_handoff_cb,NULL);
	}
	else {
		scan_start(scan_type,2,cnx_start_handoff_cb,NULL);
	}
}


void ICACHE_FLASH_ATTR
cnx_sta_connect_cmd(wl_profile_st *prof, uint8_t desChan)
{
	DPRINTF("cnx_sta_connect_cmd(%p, %d)\n", prof, desChan);
	int start_chan;
	int max_chan;
	int min_chan;
	uint32_t scan_type;

	if ((g_ic.ic_profile.led.open_flag) && (g_ic.ic_profile.opmode == STATION_MODE)) {
		ets_timer_disarm(&sta_con_timer);
		ets_timer_setfn(&sta_con_timer,cnx_sta_connect_led_timer_cb,NULL);
		ets_timer_arm_new(&sta_con_timer,50 /* ms */,true /* repeat_flag */,true /* ms_flag */);
	}
	WDEV_BASE->reg06c_recv_flag |= 0x10;
	if ((prof->sta).bssid_set == false) {
		wDev_SetRxPolicy(RX_DISABLE,'\0',NULL);
	}
	else {
		wDev_SetRxPolicy(RX_BSSID,'\0',(prof->sta).bssid);
	}
	scan_remove_bssid();
	if (desChan == 0) {
		scan_type = 0x50f;
	}
	else {
		scan_set_desChan(desChan);
		scan_type = 0x8400;
	}
	if ((g_cnxMgr.cnx_type & CONNECT_WITHOUT_SCAN) == 0) {
		scan_add_probe_ssid(0,(prof->sta).ssid.ssid,(prof->sta).ssid.len,true);
	}
	else {
		scan_type = scan_type | 0xc000;
	}
	if (scan_type >> 0xf != 0) goto LAB_40219146;
	start_chan = ieee80211_regdomain_min_chan();
	if (g_ic.ic_profile.sta.channel < start_chan) {
LAB_4021911d:
		start_chan = scan_prefer_chan((char *)g_ic.ic_profile.sta.ssid.ssid);
	}
	else {
		max_chan = ieee80211_regdomain_max_chan();
		start_chan = g_ic.ic_profile.sta.channel;
		if (max_chan < g_ic.ic_profile.sta.channel) goto LAB_4021911d;
	}
	min_chan = ieee80211_regdomain_min_chan();
	if ((start_chan < min_chan) || (max_chan = ieee80211_regdomain_max_chan(), max_chan < start_chan))
	{
		start_chan = ieee80211_regdomain_min_chan();
	}
	scan_build_chan_list(0x20000,start_chan);
LAB_40219146:
	cnx_start_connect(scan_type);
}

/*
         U auth_type
         U BcnEb_update
         U chm_acquire_lock
         U chm_get_current_channel
         U chm_release_lock
         U chm_return_home_channel
         U chm_start_op
         U dhcp_cleanup
         U dhcp_release
         U dhcp_stop
         U esp_random
         U ethbroadcast
         U ets_bzero
         U ets_intr_lock
         U ets_intr_unlock
         U ets_memcmp
         U ets_memcpy
         U ets_memset
         U ets_post
         U ets_strlen
         U ets_timer_arm_new
         U ets_timer_disarm
         U ets_timer_setfn
         U event_cb
         U g_ic
         U gpio_output_set
         U gScanStruct
         U ic_set_sta
         U ieee80211_chan2ieee
         U ieee80211_ht_updateparams
         U ieee80211_mlme_connect_bss
         U ieee80211_parse_wmeparams
         U ieee80211_phy_init
         U ieee80211_phy_type_get
         U ieee80211_regdomain_max_chan
         U ieee80211_regdomain_min_chan
         U ieee80211_regdomain_update
         U ieee80211_send_mgmt
         U ieee80211_send_probereq
         U ieee80211_set_shortslottime
         U ieee80211_setup_phy_mode
         U ieee80211_setup_rates
         U ieee80211_setup_rateset
         U ieee80211_sta_new_state
         U ieee80211_wme_updateparams
         U netif_set_down
         U open_signaling_measurement
         U os_printf_plus
         U pm_keep_active_disable
         U pm_keep_active_enable
         U ppInstallKey
         U ppRegisterTxCallback
         U pvPortZalloc
         U pwrsave_flushq
         U scan_add_bssid
         U scan_add_probe_ssid
         U scan_build_chan_list
         U scan_get_type
         U scan_hidden_ssid
         U scan_prefer_chan
         U scan_remove_bssid
         U scan_remove_probe_ssid
         U scan_set_act_duration
         U scan_set_desChan
         U scan_set_pas_duration
         U scan_start
         U sta_status_set
         U vPortFree
         U wDev_remove_KeyEntry
         U wDev_remove_KeyEntry_all_cnx
         U wDev_SetRxPolicy
         U wifi_get_opmode
         U wifi_get_phy_mode
         U wifi_station_get_config
         U wifi_station_get_reconnect_policy
         U wifi_station_set_config_current
         U wpa_auth_sta_associated
         U wpa_auth_sta_deinit
         U wpa_auth_sta_init
         U wpa_config_bss
         U wpa_config_profile
         U wpa_validate_wpa_ie
00000000 t cnx_process_handoff_trigger
00000000 t cnx_process_tim
00000000 b g_cnxMgr_84
00000000 d join_deny_flag_222
00000000 t mem_debug_file_142
00000004 t cnx_cal_rc_util
00000004 T cnx_rc_update_age
00000004 T cnx_start_handoff_cb
00000004 t ieee80211_is_11b_rate
00000004 T wifi_softap_staconnected_event_policy
00000008 T cnx_add_rc
00000008 t cnx_get_next_rc
00000008 T cnx_rc_update_rssi
00000008 T cnx_update_bss
00000008 T wifi_softap_toomany_deny
0000000c T cnx_rc_update_state_metric
0000000c T cnx_remove_all_rc
0000000c T cnx_remove_rc
0000000c t cnx_traverse_rc_lis_done
00000010 T cnx_assoc_timeout
00000010 T cnx_auth_timeout
00000010 T cnx_bss_alloc
00000010 T cnx_node_search
00000010 T cnx_rc_search
00000010 t cnx_remove_rc_except
00000010 T cnx_sta_connect_led_timer_cb
00000010 t flash_str$7209_76_4
00000014 T cnx_node_alloc
00000018 T cnx_attach
00000018 t cnx_beacon_timeout
00000018 T cnx_handshake_timeout
00000018 t cnx_probe_rc_tx_cb
00000018 t cnx_traverse_rc_list
0000001c t cnx_choose_rc
0000001c t cnx_probe_rc
0000001c t cnx_start_connect
0000001c t send_ap_probe
0000002c T cnx_node_remove
00000030 t flash_str$7374_98_2
00000034 T cnx_connect_timeout
00000038 T cnx_csa_fn
00000040 t flash_str$7447_104_10
00000040 t mgd_probe_send_timeout
00000044 T cnx_node_leave
0000004c T cnx_sta_leave
00000050 T cnx_sta_associated
0000005c t cnx_connect_op
00000070 T cnx_sta_connect_cmd
00000070 t flash_str$7517_116_2
00000080 T cnx_sta_scan_cmd
00000084 t cnx_connect_to_bss
0000008c t cnx_do_handoff
00000090 t flash_str$8393_160_1
00000098 T cnx_update_bss_more
000000a8 T cnx_node_join
000000b0 t flash_str$8399_161_1
000000f0 t flash_str$8615_169_4
00000130 t flash_str$8616_169_5
00000150 t flash_str$8723_173_6
00000170 t flash_str$8724_173_7
00000190 t flash_str$8729_173_9
000003d8 B sta_con_timer
000003ec B backup_ni_connect_status
000003f0 B g_cnx_probe_rc_list_cb
000003f4 B reconnect_flag
000003f5 B no_ap_found_index
000003f8 b cnx_csa_timer_157
*/

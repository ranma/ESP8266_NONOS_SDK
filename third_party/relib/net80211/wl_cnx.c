#include <assert.h>
#include <stdint.h>

#include "c_types.h"
#include "mem.h"
#include "osapi.h"
#include "spi_flash.h"
#include "gpio.h"

#include "lwip/netif.h"

#include "relib/relib.h"
#include "relib/esp8266.h"
#include "relib/xtensa.h"
#include "relib/ets_rom.h"

#include "relib/s/ieee80211com.h"
#include "relib/s/ieee80211_conn.h"
#include "relib/s/ieee80211_channel.h"
#include "relib/s/ieee80211_scaner.h"
#include "relib/s/ieee80211_scanparams.h"
#include "relib/s/ieee80211_rateset.h"
#include "relib/s/ieee80211_frame.h"
#include "relib/s/wl_profile.h"
#include "relib/s/station_config.h"
#include "relib/s/system_event.h"
#include "relib/s/cnx_mgr.h"
#include "relib/s/esf_buf.h"
#include "relib/s/esf_tx_desc.h"
#include "relib/s/hostapd_data.h"
#include "relib/s/wpa_authenticator.h"

#if 1
#define DPRINTF(fmt, ...) do { \
	ets_printf("[@%p] " fmt, __builtin_return_address(0), ##__VA_ARGS__); \
	while ((UART0->STATUS & 0xff0000) != 0); \
} while (0)
#else
#define DPRINTF(fmt, ...)
#endif

struct eth_addr {
	uint8_t addr[6];
};

#define g_cnxMgr g_cnxMgr_84
#define cnx_csa_timer cnx_csa_timer_157

typedef void (*cnx_traverse_rc_list_cb_t)(void *arg, STATUS status);
typedef void (*wifi_event_handler_cb_t)(System_Event_st *event);

extern ieee80211_scaner_st gScanStruct;
extern ieee80211com_st g_ic; /* ieee80211.c */
extern wifi_event_handler_cb_t event_cb; /* user_interface.c */
extern uint8_t open_signaling_measurement;
extern uint8_t auth_type;
extern struct eth_addr ethbroadcast;
extern uint8_t BcnEb_update;

#if 0
#define maybe_extern extern
#else
#define maybe_extern
#endif
maybe_extern uint8_t join_deny_flag;
maybe_extern ETSTimer sta_con_timer; /* wl_cnx.c */
maybe_extern ETSTimer cnx_csa_timer; /* wl_cnx.c */
maybe_extern cnx_mgr_st g_cnxMgr; /* wl_cnx.c:84 */
maybe_extern uint8_t backup_ni_connect_status; /* wl_cnx.c */
maybe_extern uint8_t no_ap_found_index; /* wl_cnx.c */
maybe_extern bool reconnect_flag; /* wl_cnx.c */
maybe_extern cnx_traverse_rc_list_cb_t g_cnx_probe_rc_list_cb; /* wl_cnx.c */

typedef enum phy_mode {
	PHY_MODE_11B=1,
	PHY_MODE_11G=2,
	PHY_MODE_11N=3
} phy_mode_t;

typedef enum ieee80211_reason {
	IEEE80211_REASON_UNSPECIFIED		= 1,
	IEEE80211_REASON_AUTH_EXPIRE		= 2,
	IEEE80211_REASON_AUTH_LEAVE		= 3,
	IEEE80211_REASON_ASSOC_EXPIRE		= 4,
	IEEE80211_REASON_ASSOC_TOOMANY		= 5,
	IEEE80211_REASON_NOT_AUTHED		= 6,
	IEEE80211_REASON_NOT_ASSOCED		= 7,
	IEEE80211_REASON_ASSOC_LEAVE		= 8,
	IEEE80211_REASON_ASSOC_NOT_AUTHED	= 9,
	IEEE80211_REASON_DISASSOC_PWRCAP_BAD	= 10,	/* 11h */
	IEEE80211_REASON_DISASSOC_SUPCHAN_BAD	= 11,	/* 11h */
	IEEE80211_REASON_IE_INVALID		= 13,	/* 11i */
	IEEE80211_REASON_MIC_FAILURE		= 14,	/* 11i */
	IEEE80211_REASON_4WAY_HANDSHAKE_TIMEOUT	= 15,	/* 11i */
	IEEE80211_REASON_GROUP_KEY_UPDATE_TIMEOUT = 16,	/* 11i */
	IEEE80211_REASON_IE_IN_4WAY_DIFFERS	= 17,	/* 11i */
	IEEE80211_REASON_GROUP_CIPHER_INVALID	= 18,	/* 11i */
	IEEE80211_REASON_PAIRWISE_CIPHER_INVALID= 19,	/* 11i */
	IEEE80211_REASON_AKMP_INVALID		= 20,	/* 11i */
	IEEE80211_REASON_UNSUPP_RSN_IE_VERSION	= 21,	/* 11i */
	IEEE80211_REASON_INVALID_RSN_IE_CAP	= 22,	/* 11i */
	IEEE80211_REASON_802_1X_AUTH_FAILED	= 23,	/* 11i */
	IEEE80211_REASON_CIPHER_SUITE_REJECTED	= 24,	/* 11i */
} ieee80211_reason_t;

typedef enum ieee80211_status {
	IEEE80211_STATUS_SUCCESS		= 0,
	IEEE80211_STATUS_UNSPECIFIED		= 1,
	IEEE80211_STATUS_CAPINFO		= 10,
	IEEE80211_STATUS_NOT_ASSOCED		= 11,
	IEEE80211_STATUS_OTHER			= 12,
	IEEE80211_STATUS_ALG			= 13,
	IEEE80211_STATUS_SEQUENCE		= 14,
	IEEE80211_STATUS_CHALLENGE		= 15,
	IEEE80211_STATUS_TIMEOUT		= 16,
	IEEE80211_STATUS_TOOMANY		= 17,
	IEEE80211_STATUS_BASIC_RATE		= 18,
	IEEE80211_STATUS_SP_REQUIRED		= 19,	/* 11b */
	IEEE80211_STATUS_PBCC_REQUIRED		= 20,	/* 11b */
	IEEE80211_STATUS_CA_REQUIRED		= 21,	/* 11b */
	IEEE80211_STATUS_SPECMGMT_REQUIRED	= 22,	/* 11h */
	IEEE80211_STATUS_PWRCAP_REQUIRED	= 23,	/* 11h */
	IEEE80211_STATUS_SUPCHAN_REQUIRED	= 24,	/* 11h */
	IEEE80211_STATUS_SHORTSLOT_REQUIRED	= 25,	/* 11g */
	IEEE80211_STATUS_DSSSOFDM_REQUIRED	= 26,	/* 11g */
	IEEE80211_STATUS_INVALID_IE		= 40,	/* 11i */
	IEEE80211_STATUS_GROUP_CIPHER_INVALID	= 41,	/* 11i */
	IEEE80211_STATUS_PAIRWISE_CIPHER_INVALID = 42,	/* 11i */
	IEEE80211_STATUS_AKMP_INVALID		= 43,	/* 11i */
	IEEE80211_STATUS_UNSUPP_RSN_IE_VERSION	= 44,	/* 11i */
	IEEE80211_STATUS_INVALID_RSN_IE_CAP	= 45,	/* 11i */
	IEEE80211_STATUS_CIPHER_SUITE_REJECTED	= 46,	/* 11i */
} ieee80211_status_t;

typedef enum ieee80211_phytype {
	IEEE80211_T_CCK=0,
	IEEE80211_T_OFDM=1,
	IEEE80211_T_HT20_L=2,
	IEEE80211_T_HT20_S=3
} ieee80211_phytype_t;

typedef enum roam_trigger_t {
	BSS_AGE_METRIC=1,
	CONNECTION_STATE_METRIC=2,
	RSSI_AVG_METRIC=4,
	RSSIDT_METRIC=8,
	PMKID_METRIC=16,
	HOST_REQUEST=32,
	BMISS_EVENT=64,
	DISCONNECT_EVENT=128,
	LOW_RSSI_EVENT=256,
	ROAM_BIAS=512,
	BSS_CHANNEL_CHANGE=1024,
	PERIODIC_SEARCH_COMPLETE=2048
} roam_trigger_t;

typedef enum metric_flag_t {
	METRIC_RESET=0,
	METRIC_UPDATE=1,
	METRIC_AVERAGE=2
} metric_flag_t;

typedef enum bcn_policy {
	RX_DISABLE=0,
	RX_ANY=1,
	RX_BSSID=2,
} bcn_policy_t;

typedef enum wpa_alg {
	WPA_ALG_NONE=0,
	WPA_ALG_WEP40=1,
	WPA_ALG_TKIP=2,
	WPA_ALG_CCMP=3,
	WPA_ALG_WAPI=4,
	WPA_ALG_WEP104=5,
	WPA_ALG_WEP=6,
	WPA_ALG_IGTK=7,
	WPA_ALG_PMK=8,
	WPA_ALG_GCMP=9
} wpa_alg_t;

struct wpa2_funcs {
	int (*wpa2_sm_rx_eapol)(void);
	int (*wpa2_start)(void);
	int (*wpa2_init)(void);
	void (*wpa2_deinit)(void);
};

struct wps_funcs {
	bool (*wps_parse_scan_result)(void);
	void (*wifi_station_wps_start)(void);
	int (*wps_sm_rx_eapol)(void);
	int (*wps_start_pending)(void);
};

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

void cnx_sta_connect_cmd(wl_profile_st *prof, uint8_t desChan);
int ieee80211_sta_new_state(ieee80211com_st *ic, ieee80211_state_t nstate, int arg);

void ICACHE_FLASH_ATTR
cnx_connect_timeout()
{
	ieee80211_conn_st *conn = g_ic.ic_if0_conn;

	os_printf_plus("reconnect\n");
	if (conn->ni_mlme_state != IEEE80211_S_INIT) {
		reconnect_flag = true;
		ieee80211_sta_new_state(&g_ic,IEEE80211_S_INIT,0);
		reconnect_flag = false;
	}
	uint32_t chancfg = RTCMEM->SYSTEM_CHANCFG;
	if (chancfg & 0x10000) {
		if ((chancfg & 0xff) < 0xe) {
			cnx_sta_connect_cmd(&g_ic.ic_profile,chancfg & 0xff);
		}
		else {
			cnx_sta_connect_cmd(&g_ic.ic_profile,0);
		}
	}
	else {
		cnx_sta_connect_cmd(&g_ic.ic_profile,0);
	}
}

ieee80211_channel_st *chm_get_current_channel(void);
STATUS chm_start_op(ieee80211_channel_st *ch, uint32_t min_duration, uint32_t max_duration, void *start_fn, void *end_fn, void *arg);
STATUS chm_acquire_lock(uint8_t priority, cnx_traverse_rc_list_cb_t cb, void *arg);
void chm_release_lock(void);
int16_t ieee80211_chan2ieee(ieee80211_channel_st *c);
STATUS cnx_connect_to_bss(ieee80211_bss_st *bss);
void wDev_remove_KeyEntry(int key_entry_valid);
void ppInstallKey(wpa_alg_t alg, uint8_t *addr, int key_idx, int set_tx, uint8_t *seq, uint32_t seq_len, uint8_t *key, uint32_t key_len, int key_entry_valid);
void wpa_config_profile(ieee80211com_st *ic);
void wpa_config_bss(ieee80211com_st *ic, ieee80211_bss_st *bss);
uint32_t scan_get_type(void);
void ieee80211_mlme_connect_bss(ieee80211_bss_st *bss, wl_profile_st *conn_profile, bool reconnect, bool doAuth);

void ICACHE_FLASH_ATTR
cnx_connect_op(ieee80211_bss_st *bss, STATUS status)
{
	chm_release_lock();
	if (status != OK) {
		return;
	}

	wDev_remove_KeyEntry(2);
	wDev_remove_KeyEntry(3);
	wDev_remove_KeyEntry(6);
	wDev_remove_KeyEntry(7);
	wDev_remove_KeyEntry(8);
	wDev_remove_KeyEntry(9);

	if (g_ic.ic_profile.sta.authmode == '\x01') {
		uint8_t seq[8] = {0};
		uint8_t *wepkey = g_ic.ic_profile.sta.wepkey;

		if (g_ic.ic_profile.sta.pairwise_cipher == '\a') {
			ppInstallKey(WPA_ALG_WEP40,bss->ni_bssid,0,1,seq,8,wepkey,5,5);
			ppInstallKey(WPA_ALG_WEP40,bss->ni_bssid,1,1,seq,8,wepkey,5,6);
		}
		else {
			if (g_ic.ic_profile.sta.pairwise_cipher != '\b') goto LAB_4021999a;
			ppInstallKey(WPA_ALG_WEP104,bss->ni_bssid,0,1,seq,8,wepkey,0xd,5);
			ppInstallKey(WPA_ALG_WEP104,bss->ni_bssid,1,1,seq,8,wepkey,0xd,6);
		}
		bss->ni_gtk_index = '\x04';
	}
	else {
		wpa_config_profile(&g_ic);
		wpa_config_bss(&g_ic,bss);
		bss->ni_gtk_index = 0xff;
		bss->ni_gtk_index_array[0] = 0xff;
		bss->ni_gtk_index_array[1] = 0xff;
		bss->ni_gtk_index_array[2] = 0xff;
		bss->ni_gtk_index_array[3] = 0xff;
	}
LAB_4021999a:
	scan_get_type();
	bool reconnect = true;
	if ((g_cnxMgr.cnx_last_bss == NULL) ||
		 (g_cnxMgr.cnx_last_bss->ni_bssid[0] == '\0' &&
		  g_cnxMgr.cnx_last_bss->ni_bssid[1] == '\0' &&
		  g_cnxMgr.cnx_last_bss->ni_bssid[2] == '\0' &&
		  g_cnxMgr.cnx_last_bss->ni_bssid[3] == '\0' &&
		  g_cnxMgr.cnx_last_bss->ni_bssid[4] == '\0' &&
		  g_cnxMgr.cnx_last_bss->ni_bssid[5] == '\0')) {
		reconnect = false;
	}
	if ((reconnect) && (g_cnxMgr.cnx_authMode != '\0')) {
		ieee80211_mlme_connect_bss(bss,&g_ic.ic_profile,reconnect,false);
	}
	else {
		ieee80211_mlme_connect_bss(bss,&g_ic.ic_profile,reconnect,true);
	}
}

STATUS ICACHE_FLASH_ATTR
cnx_csa_fn(ieee80211_bss_st *bss)
{
	os_printf_plus("switch to channel %d\n", g_ic.ic_csa_chan);
	if (bss == NULL) {
		if (chm_acquire_lock('\x03',NULL,NULL) != OK) {
			return FAIL;
		}
		ieee80211_channel_st *chan = &g_ic.ic_channels[g_ic.ic_csa_chan];
		if (chm_start_op(chan,0,0,NULL,NULL,NULL) == OK) {
			uint32_t saved = LOCK_IRQ_SAVE();
			if (g_ic.ic_home_channel != chan) {
				BcnEb_update = '\x01';
			}
			g_ic.ic_home_channel = chan;
			LOCK_IRQ_RESTORE(saved);
		}
	}
	else {
		cnx_connect_to_bss(bss);
	}
	g_ic.ic_csa_state = '\0';
	return OK;
}

uint32_t esp_random(void);

STATUS ICACHE_FLASH_ATTR
cnx_connect_to_bss(ieee80211_bss_st *bss)
{
	ieee80211_channel_st *current_chan = chm_get_current_channel();

	if (((g_cnxMgr.cnx_cur_bss == NULL) || (bss != g_cnxMgr.cnx_cur_bss)) || (current_chan != bss->ni_channel)) {
		if (bss == NULL) {
			return FAIL;
		}
		uint32_t saved = LOCK_IRQ_SAVE();
		if (((g_ic.ic_profile.opmode == 3) && (g_ic.ic_bss_num != 0)) && (g_ic.ic_csa_state != 1)) {
			if (current_chan != bss->ni_channel) {
				g_ic.ic_csa_state = 1;
				g_ic.ic_csa_cnt = 3;
				g_ic.ic_ext_ap = bss;
				g_ic.ic_csa_chan = ieee80211_chan2ieee(bss->ni_channel);
				LOCK_IRQ_RESTORE(saved);
				ets_timer_disarm(&cnx_csa_timer);
				ets_timer_setfn(&cnx_csa_timer,(os_timer_func_t*)cnx_csa_fn,bss);
				ets_timer_arm_new(&cnx_csa_timer,(g_ic.ic_csa_cnt + 1) * bss->ni_intval,false,true);
				return OK;
			}
		}
		LOCK_IRQ_RESTORE(saved);

		bss->ni_associd = 0;
		uint32_t rndval;
		do {
			rndval = esp_random() & 0xfff;
			bss->ni_txseqs[0x10] = rndval;
		} while (rndval == 0);
		bss->ni_ptk_index = '\x04';

		saved = LOCK_IRQ_SAVE();
		current_chan = bss->ni_channel;
		if (g_ic.ic_home_channel != current_chan) {
			BcnEb_update = '\x01';
			current_chan = bss->ni_channel;
		}
		g_ic.ic_home_channel = current_chan;
		LOCK_IRQ_RESTORE(saved);

		g_cnxMgr.cnx_roam_start = FRC2->COUNT;
		if (bss->ni_channel == chm_get_current_channel()) {
			cnx_connect_op(bss,OK);
		}
		else {
			if (chm_acquire_lock('\x03',(cnx_traverse_rc_list_cb_t)cnx_connect_op,bss) != OK) {
				return FAIL;
			}
			chm_start_op(bss->ni_channel,0,0,cnx_connect_op,cnx_connect_op,bss);
		}
		return PENDING;
	}
	else {
		g_cnxMgr.cnx_roam_trigger = g_cnxMgr.cnx_roam_trigger | 0x140;
		(g_ic.ic_if0_conn)->ni_connect_status = backup_ni_connect_status;
		return OK;
	}
}

STATUS cnx_traverse_rc_list(cnx_traverse_rc_list_cb_t cb, void *arg);
void cnx_remove_rc_except(ieee80211_bss_st *bss);
struct ieee80211_bss *cnx_choose_rc(void);
bool wifi_station_get_config(station_config_st *config);
bool wifi_station_set_config_current(station_config_st *config);
bool wifi_station_get_reconnect_policy(void);
void sta_status_set(uint8_t status);

STATUS ICACHE_FLASH_ATTR
cnx_do_handoff()
{
	STATUS res;
	station_config_st station_cfg;

	g_cnxMgr.cnx_state = g_cnxMgr.cnx_state | CNX_S_ROAMING;

	ieee80211_conn_st *conn = g_ic.ic_if0_conn;
	ieee80211_bss_st *bss = cnx_choose_rc();
	if (bss == NULL) {
		no_ap_found_index++;
		if ((no_ap_found_index == 5) && (wifi_station_get_config(&station_cfg), station_cfg.bssid_set == true)) {
			station_cfg.bssid_set = false;
			wifi_station_set_config_current(&station_cfg);
		}
		System_Event_st *sev;
		if ((event_cb != NULL) && (sev = os_zalloc(sizeof(*sev)), sev != NULL)) {
			sev->event = EVENT_STAMODE_DISCONNECTED;
			sev->event_info.disconnected.reason = 0xc9;
			memset(sev->event_info.disconnected.bssid,0,6);
			memcpy(sev->event_info.disconnected.ssid,g_ic.ic_profile.sta.ssid.ssid,0x20);
			sev->event_info.disconnected.ssid_len = g_ic.ic_profile.sta.ssid.len;
			if (ets_post(PRIO_EVENT,0xc9,(ETSParam)sev) != 0) {
				os_free(sev);
			}
		}
		if (((g_ic.ic_if0_conn)->ni_connect_step != '\0') ||
			 ((g_ic.ic_if0_conn)->ni_connect_status != '\0')) {
			if ((g_ic.ic_if0_conn)->ni_connect_status != '\x02') {
				sta_status_set('\x03');
				if (wifi_station_get_reconnect_policy()) {
					os_printf_plus("no %s found, reconnect after 1s\n");
				}
			}
			RTCMEM->SYSTEM_CHANCFG = 0x10000;
			if (wifi_station_get_reconnect_policy()) {
				ETSTimer *connect_timer = &conn->ni_connect_timeout;
				ets_timer_disarm(connect_timer);
				ets_timer_setfn(connect_timer,cnx_connect_timeout,NULL);
				ets_timer_arm_new(connect_timer,1000,false,true);
			}
		}
		res = OK;
	}
	else {
		cnx_remove_rc_except(bss);
		if ((g_ic.ic_if0_conn)->ni_connect_status == '\x03') {
			(g_ic.ic_if0_conn)->ni_connect_status = '\x01';
			(g_ic.ic_if0_conn)->ni_connect_err_time = '\0';
		}
		res = cnx_connect_to_bss(bss);
		if (res == OK) {
			g_cnxMgr.cnx_state = g_cnxMgr.cnx_state & ~CNX_S_ROAMING;
		}
		else if (res == FAIL) {
			g_cnxMgr.cnx_roam_trigger = g_cnxMgr.cnx_roam_trigger | 0x140;
		}
	}
	return res;
}

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

int8_t ICACHE_FLASH_ATTR
cnx_add_rc(ieee80211_bss_st *bss)
{
	int list_size = g_cnxMgr.cnx_rc_list_size;
	if (2 < list_size) {
		return -1;
	}
	if (1 < list_size) {
		int i, j;
		for (i = 0; i < list_size; i++) {
			if (g_cnxMgr.cnx_rc_list[i]->ni_channel == bss->ni_channel) break;
		}
		for (j = list_size; i + 1 < j; j--) {
			g_cnxMgr.cnx_rc_list[j] = g_cnxMgr.cnx_rc_list[j - 1];
		}
		bool bVar1 = list_size != i;
		list_size = i;
		if (bVar1) {
			g_cnxMgr.cnx_rc_list[i + 1] = bss;
			goto LAB_40219cb7;
		}
	}
	g_cnxMgr.cnx_rc_list[list_size] = bss;
LAB_40219cb7:
	g_cnxMgr.cnx_rc_list_size++;
	return 0;
}

void cnx_remove_rc_except(ieee80211_bss_st *bss)
{
	int list_size = g_cnxMgr.cnx_rc_list_size;
	if (list_size != 0) {
		for (int i = 0; i < list_size; i++) {
			if (g_cnxMgr.cnx_rc_list[i] != bss) {
				bzero(g_cnxMgr.cnx_rc_list[i],sizeof(*g_cnxMgr.cnx_rc_list[i]));
				g_cnxMgr.cnx_rc_list[i] = NULL;
			}
		}
	}
	g_cnxMgr.cnx_rc_list[0] = bss;
	g_cnxMgr.cnx_rc_list_size = 1;
}

uint32_t ICACHE_FLASH_ATTR
cnx_cal_rc_util(ieee80211_bss_st *bss)
{
	uint32_t res;

	if ((g_cnxMgr.cnx_rc_weight & 1) == 0) {
		if ((g_cnxMgr.cnx_rc_weight >> 1 & 1) == 0) {
			res = 0;
		}
		else {
			res = (uint32_t)(bss->ni_rc_metrics).rssi_avg;
		}
	}
	else {
		int8_t iVar1 = (bss->ni_rc_metrics).cs_utility;
		res = (uint32)iVar1;
		if ((g_cnxMgr.cnx_rc_weight >> 1 & 1) != 0) {
			res = (int)(short)iVar1 * (int)(short)(bss->ni_rc_metrics).rssi_avg;
		}
	}
	if ((((g_cnxMgr.cnx_rc_weight >> 2 & 1) != 0) && (res != 0)) &&
		 ((bss->ni_rc_metrics).rssi_dt != 0)) {
		res = res + 2;
	}
	if (((g_cnxMgr.cnx_rc_weight >> 3 & 1) != 0) && ((bss->ni_rc_metrics).pmkid_valid != false)) {
		res = res << 1;
	}
	return res;
}

ieee80211_bss_st* ICACHE_FLASH_ATTR
cnx_choose_rc(void)
{
	if (g_cnxMgr.cnx_rc_list_size == '\0') {
		return NULL;
	}

	ieee80211_bss_st *res = NULL;
	uint32_t max_utility = 0;
	int list_size= g_cnxMgr.cnx_rc_list_size;
	res = NULL;
	for (int i = 0; i < list_size; i++) {
		ieee80211_bss_st *bss = g_cnxMgr.cnx_rc_list[i];
		if ((g_ic.ic_profile.sta.bssid_set != false) &&
			 (memcmp(g_ic.ic_profile.sta.bssid,bss,6) == 0)) {
			return bss;
		}
		uint32_t utility = cnx_cal_rc_util(bss);
		if (max_utility < utility) {
			max_utility = utility;
			res = bss;
		}
	}
	return res;
}


ieee80211_bss_st* ICACHE_FLASH_ATTR
cnx_rc_search(uint8_t *bssid)
{
	int size = g_cnxMgr.cnx_rc_list_size;
	for (int i = 0; i < size; i++) {
		ieee80211_bss_st *bss = g_cnxMgr.cnx_rc_list[i];
		if (bss != NULL) {
			if (memcmp(bss,bssid,6) == 0) {
				return bss;
			}
		}
	}
	return NULL;
}

void ICACHE_FLASH_ATTR
cnx_remove_all_rc(void)
{
	int list_size = g_cnxMgr.cnx_rc_list_size;
	
	for (int i = 0; i < list_size; i++) {
		bzero(g_cnxMgr.cnx_rc_list[i],sizeof(*g_cnxMgr.cnx_rc_list[i]));
		g_cnxMgr.cnx_rc_list[i] = NULL;
	}
	g_cnxMgr.cnx_rc_list_size = '\0';
}

void ICACHE_FLASH_ATTR
cnx_remove_rc(ieee80211_bss_st *bss)
{
	int size = g_cnxMgr.cnx_rc_list_size;
	int i = 0;

	for (; i < size; i++) {
		if (g_cnxMgr.cnx_rc_list[i] == bss) {
			bzero(bss,sizeof(*bss));
			g_cnxMgr.cnx_rc_list[i] = NULL;
			g_cnxMgr.cnx_rc_list_size--;
			break;
		}
	};

	for (; i < size - 1; i++) {
		g_cnxMgr.cnx_rc_list[i] = g_cnxMgr.cnx_rc_list[i + 1];
	}
}

ieee80211_bss_st* ICACHE_FLASH_ATTR
cnx_get_next_rc(void)
{
	int idx = g_cnxMgr.cnx_rc_index;
	int size = g_cnxMgr.cnx_rc_list_size;
	for (; idx < size; idx++, g_cnxMgr.cnx_rc_index++) {
		ieee80211_bss_st *bss = g_cnxMgr.cnx_rc_list[idx];
		if ((bss->ni_channel->ch_flags >> 9 & 1) == 0) {
			return bss;
		}
	}
	return NULL;
}

int ieee80211_send_probereq(ieee80211_bss_st *bss, uint8_t *sa, uint8_t *da, uint8_t *bssid, uint8_t *ssid, uint8_t ssidlen, uint8_t tx_cb_id);

void ICACHE_FLASH_ATTR
cnx_probe_rc(ieee80211_bss_st *bss,STATUS status)
{
	struct netif *ni = (g_ic.ic_if0_conn)->ni_ifp;
	ieee80211_channel_st *chan = chm_get_current_channel();

	if (chan == bss->ni_channel) {
		ieee80211_send_probereq(bss,ni->hwaddr,bss->ni_bssid,bss->ni_bssid,
					 g_ic.ic_profile.sta.ssid.ssid,g_ic.ic_profile.sta.ssid.len,0);
	}
	else {
		chm_start_op(bss->ni_channel,0,0,cnx_probe_rc,NULL,bss);
	}
}

void ICACHE_FLASH_ATTR __attribute__((weak))
cnx_process_handoff_trigger(ieee80211_bss_st *bss,roam_trigger_t trigger)
{
}

void ICACHE_FLASH_ATTR
cnx_rc_update_age(ieee80211_bss_st *bss,int val,metric_flag_t flag)
{
	int new = (bss->ni_rc_metrics).bss_age;
	switch (flag) {
	case METRIC_RESET: new = val; break;
	case METRIC_UPDATE: new += val; break;
	}
	(bss->ni_rc_metrics).bss_age = new;
	if (new == 0) {
		cnx_process_handoff_trigger(bss,BSS_AGE_METRIC);
	}
}

void ICACHE_FLASH_ATTR
cnx_rc_update_state_metric(ieee80211_bss_st *bss,int val,metric_flag_t flag)
{
	int8_t new_utility = (bss->ni_rc_metrics).cs_utility;

	switch (flag) {
	case METRIC_UPDATE: new_utility += val;
	case METRIC_RESET: new_utility = val;
	}

	if (new_utility < 0) {
		new_utility = 0;
	}
	int max_utility = 10;
	if (g_cnxMgr.cnx_cur_bss == bss) {
		max_utility = 15;
	}
	if (max_utility < new_utility) {
		new_utility = max_utility;
	}

	(bss->ni_rc_metrics).cs_utility = new_utility;
	if (new_utility == 0) {
		if (g_cnxMgr.cnx_cur_bss == bss) {
			cnx_process_handoff_trigger(bss,CONNECTION_STATE_METRIC);
		}
		else {
			cnx_remove_rc(bss);
		}
	}
}

void ICACHE_FLASH_ATTR
cnx_rc_update_rssi(ieee80211_bss_st *bss,int rssi,metric_flag_t flag)
{
	(bss->ni_rc_metrics).rssi_last = rssi;
	int rssi_avg = (bss->ni_rc_metrics).rssi_avg;
	int new_avg = rssi;
	if (flag != METRIC_RESET && rssi_avg != 0) {
		int tmp_sum = rssi * 3 + rssi_avg * 13;
		if (tmp_sum < 0) {
			tmp_sum += 0xf;
		}
		new_avg = (int8_t)(tmp_sum >> 4);
	}
	if (rssi_avg != 0) {
		int new_delta = new_avg - rssi_avg;
		int old_delta = (bss->ni_rc_metrics).rssi_dt;
		int tmp_sum = old_delta * 10 + new_delta * 6;
		if (tmp_sum < 0) {
			tmp_sum += 0xf;
		}
		if (old_delta != 0) {
			new_delta = (int8_t)(tmp_sum >> 4);
		}
		(bss->ni_rc_metrics).rssi_dt = new_delta;
	}
	(bss->ni_rc_metrics).rssi_avg = new_avg;
	if (new_avg == 0) {
		cnx_process_handoff_trigger(bss,RSSI_AVG_METRIC);
	}
	if ((bss->ni_rc_metrics).rssi_dt < 0) {
		cnx_process_handoff_trigger(bss,RSSIDT_METRIC);
	}
}

int ieee80211_setup_rateset(ieee80211_rateset_st *rs, uint8_t *rates, uint8_t *xrates, int flags);

void ICACHE_FLASH_ATTR
cnx_update_bss(ieee80211_bss_st *bss,ieee80211_scanparams_st *scan,ieee80211_frame_st *wh)
{
	memcpy(bss,wh->i_addr3,6);
	ieee80211_setup_rateset(&bss->ni_rates,scan->rates,scan->xrates,0xf);
}

static uint8_t lookup_11b_rate[4] = {2, 4, 0xb, 0x16};

bool ICACHE_FLASH_ATTR
ieee80211_is_11b_rate(ieee80211_scanparams_st *scan)
{
	int rate0 = scan->rates[0];
	if (4 < rate0) {
		return false;
	}
	if (rate0 != 0) {
		for (int i = 0; i < rate0; i++) {
			int found = false;
			for (int j = 0; j < 4; j++) {
				if ((scan->rates[i + 1] & 0x7f) == lookup_11b_rate[j]) {
					found = true;
					break;
				}
			}
			if (!found) {
				return false;
			}
		}
	}
	return true;
}


struct chanAccParams;

void ICACHE_FLASH_ATTR __attribute__((weak))
cnx_process_tim(ieee80211_bss_st *bss, uint8_t *ie_tim)
{
}

void ieee80211_set_shortslottime(ieee80211com_st * ic, int onoff);
int ieee80211_setup_rates(ieee80211_bss_st * bss, uint8_t * rates, uint8_t * xrates, int flags);
void ieee80211_setup_phy_mode(ieee80211_bss_st * bss, bool is_ht);
int ieee80211_parse_wmeparams(ieee80211_bss_st * bss, uint8_t * frm);
void ieee80211_wme_updateparams(ieee80211_bss_st * bss, struct chanAccParams * wmeAcParams);
bool ieee80211_regdomain_update(ieee80211_bss_st * bss, uint8_t * frm);
bool ieee80211_is_11b_rate (ieee80211_scanparams_st * scan);
phy_mode_t wifi_get_phy_mode(void);
void ieee80211_phy_init(ieee80211_phymode_t phyMode);
void ieee80211_ht_updateparams(ieee80211_conn_st * bss, uint8_t * htcapie, uint8_t * htinfoie);

void ICACHE_FLASH_ATTR
cnx_update_bss_more(ieee80211_bss_st *bss,ieee80211_scanparams_st *scan,bool isProbe)
{
	bool is_current = g_cnxMgr.cnx_cur_bss == bss;
	if (bss->ni_intval != scan->bintval) {
		bss->ni_intval = scan->bintval;
	}
	if ((!isProbe) && (scan->tim != NULL)) {
		if (is_current) {
			cnx_process_tim(bss,scan->tim);
		}
		uint8_t dtim_period = scan->tim[3];
		bss->ni_dtim_count = scan->tim[2];
		bss->ni_dtim_period = dtim_period;
	}
	uint8_t *tstamp = scan->tstamp;
	bss->ni_erp = scan->erp;
	memcpy(&bss->ni_tstamp,tstamp,8);
	uint16_t capinfo = scan->capinfo;
	if ((((bss->ni_capinfo ^ capinfo) >> 10 & 1) != 0) && (is_current)) {
		ieee80211_set_shortslottime(&g_ic,capinfo & 0x400);
		capinfo = scan->capinfo;
	}
	bss->ni_capinfo = capinfo;
	if (open_signaling_measurement != 0) {
		ieee80211_setup_rates(bss,scan->rates,scan->xrates,0);
		uint8_t *htcap = scan->htcap;
		ieee80211_setup_phy_mode(bss,htcap != NULL);
	}
	if (scan->wme == NULL) {
		if ((bss->ni_chan_ac_params).cap_info != 0) {
			bzero(&bss->ni_chan_ac_params,sizeof(bss->ni_chan_ac_params));
		}
	}
	else {
		int parse_ok = ieee80211_parse_wmeparams(bss,scan->wme);
		if ((0 < parse_ok) && (is_current)) {
			ieee80211_wme_updateparams(bss,&bss->ni_chan_ac_params);
		}
	}
	ieee80211_regdomain_update(bss,scan->country);

	ieee80211_setup_rates(bss,scan->rates,scan->xrates,0);
	if (ieee80211_is_11b_rate(scan) || (wifi_get_phy_mode() == PHY_MODE_11B)) {
		ieee80211_phy_init(IEEE80211_MODE_11B);
	}
	else {
		ieee80211_phy_init(IEEE80211_MODE_11G);
	}

	/* Inconsistent dupe? */
	ieee80211_setup_rates(bss,scan->rates,scan->xrates,0);
	if (ieee80211_is_11b_rate(scan)) {
		ieee80211_phy_init(IEEE80211_MODE_11B);
	}
	else {
		ieee80211_phy_init(IEEE80211_MODE_11G);
	}

	uint8_t *htcap = scan->htcap;
	uint8_t *htinfo = scan->htinfo;
	if (((htcap != NULL) && (htinfo != NULL)) &&
		 ((g_ic.ic_profile.flags_ht >> 0x13 & 1) != 0)) {
		(g_ic.ic_if0_conn)->ni_channel = bss->ni_channel;
		ieee80211_ht_updateparams(g_ic.ic_if0_conn,htcap,htinfo);
	}

	System_Event_st *sev;
	if ((((auth_type & 0xf) != auth_type >> 4) && (event_cb != NULL)) &&
		 (sev = os_zalloc(sizeof(*sev)), sev != NULL)) {
		sev->event = EVENT_STAMODE_AUTHMODE_CHANGE;
		Event_StaMode_AuthMode_Change_st *info = &sev->event_info.auth_change;
		info->old_mode = auth_type >> 4;
		info->new_mode = auth_type & 0xf;
		if (ets_post(PRIO_EVENT,EVENT_STAMODE_AUTHMODE_CHANGE,(ETSParam)sev) != OK) {
			os_free(sev);
		}
	}
	if ((((gScanStruct.ss_state & 1) == 0) && ((auth_type & 0xf) != auth_type >> 4)) &&
		 ((wifi_station_get_reconnect_policy() &&
			(g_ic.ic_profile.wps_status == WPS_STATUS_DISABLE)))) {
		ETSTimer *connect_timeout = &(g_ic.ic_if0_conn)->ni_connect_timeout;
		ets_timer_disarm(connect_timeout);
		ets_timer_setfn(connect_timeout,cnx_connect_timeout,NULL);
		ets_timer_arm_new(connect_timeout,1000,false,true);
	}
}


void chm_return_home_channel(void);
void chm_release_lock(void);

void ICACHE_FLASH_ATTR
cnx_traverse_rc_lis_done(void *unused_arg,STATUS status)
{
	cnx_traverse_rc_list_cb_t cb = g_cnx_probe_rc_list_cb;
	g_cnxMgr.cnx_rc_index = 0;
	chm_return_home_channel();
	chm_release_lock();
	if (cb != NULL) {
		cb(NULL,status);
	}
}

STATUS ICACHE_FLASH_ATTR
cnx_traverse_rc_list(cnx_traverse_rc_list_cb_t cb,void *arg)
{
	g_cnx_probe_rc_list_cb = cb;
	if (chm_acquire_lock(2 /* priority */,cnx_traverse_rc_lis_done,NULL) != OK) {
		return FAIL;
	}
	ieee80211_bss_st *bss = cnx_get_next_rc();
	if (bss == NULL) {
		cnx_traverse_rc_lis_done(NULL,OK);
	}
	else {
		cnx_probe_rc(bss,OK);
	}
	return PENDING;
}

typedef void (*pp_tx_cb_t)(esf_buf_st *eb);

void ICACHE_FLASH_ATTR
cnx_probe_rc_tx_cb(esf_buf_st *eb)
{
	ieee80211_bss_st *bss = cnx_rc_search(eb->buf_begin + 0x10);
	if (bss != NULL) {
		if (((eb->desc).tx_desc)->status == 1) {
			cnx_rc_update_age(bss,5,METRIC_RESET);
		}
		else {
			cnx_rc_update_state_metric(bss,-3,METRIC_UPDATE);
		}
	}
	bss = cnx_get_next_rc();
	if (bss == NULL) {
		cnx_traverse_rc_lis_done(NULL,OK);
	}
	else {
		cnx_probe_rc(bss,OK);
	}
}

ieee80211_bss_st* ICACHE_FLASH_ATTR
cnx_node_alloc(uint8_t *bssid)
{
	ieee80211_conn_st *conn = g_ic.ic_if1_conn;
	int max_conn = g_ic.ic_profile.softap.max_connection;
	for (int i = 1; i < max_conn + 1; i++) {
		if ((g_ic.ic_if1_conn)->ni_ap_bss[i] == NULL) {
			ieee80211_bss_st *bss = os_zalloc(sizeof(*bss));
			if (bss == NULL) {
				return NULL;
			}
			memcpy(bss->ni_bssid,bssid,6);
			bss->ni_ptk_index = i + '\a';
			conn->ni_ap_bss[i] = bss;
			return bss;
		}
	}
	return NULL;
}

void ICACHE_FLASH_ATTR
cnx_assoc_timeout(void)
{
	ieee80211_sta_new_state(&g_ic,IEEE80211_S_INIT,4);
	if (wifi_station_get_reconnect_policy()) {
		cnx_connect_timeout();
	}
}

void ICACHE_FLASH_ATTR
cnx_auth_timeout(void)
{
	ieee80211_sta_new_state(&g_ic,IEEE80211_S_INIT,2);
	if (wifi_station_get_reconnect_policy()) {
		cnx_connect_timeout();
	}
}

ieee80211_bss_st* ICACHE_FLASH_ATTR
cnx_bss_alloc(uint8_t *bssid,int rssi,uint8_t authtype)
{
	if ((rssi < g_ic.ic_profile.minimum_rssi_in_fast_scan) ||
		 (authtype < g_ic.ic_profile.minimum_auth_mode_in_fast_scan)) {
		return NULL;
	}

	ieee80211_bss_st *bss = NULL;
	uint32_t max_rc = 0xffffffff;
	for (int i = 0; i < ARRAY_SIZE(g_cnxMgr.cnx_bss_table); i++) {
		ieee80211_bss_st *tmp = &g_cnxMgr.cnx_bss_table[i];
		uint8_t *bssid = tmp->ni_bssid;
		if (bssid[0] == 0 && bssid[1] == 0 && bssid[2] == 0 && bssid[3] == 0 && bssid[4] == 0 && bssid[5] == 0) {
			bss = tmp;
			break;
		}
		int rc_val;
		if ((g_cnxMgr.cnx_cur_bss != tmp) && (rc_val = cnx_cal_rc_util(tmp), rc_val < max_rc)) {
			bss = tmp;
			max_rc = rc_val;
		}
	}
	if (((((bss->ni_bssid[0] != '\0') || (bss->ni_bssid[1] != '\0')) || (bss->ni_bssid[2] != '\0'))
			|| ((bss->ni_bssid[3] != '\0' || (bss->ni_bssid[4] != '\0')))) || (bss->ni_bssid[5] != '\0')
		 ) {
		cnx_remove_rc(bss);
	}
	bss->bss_flags = bss->bss_flags | 1;

	return bss;
}


struct wpa_state_machine;

void wDev_remove_KeyEntry(int key_entry_valid);
void wpa_auth_sta_deinit(struct wpa_state_machine *sm);
void pwrsave_flushq(ieee80211_bss_st *bss, int timeout);

ieee80211_bss_st* ICACHE_FLASH_ATTR
cnx_node_search(uint8_t *bssid)
{
	ieee80211_conn_st *conn = g_ic.ic_if1_conn;

	if (bssid[0] & 1) {
		return conn->ni_ap_bss[0];
	}

	int max_conn = g_ic.ic_profile.softap.max_connection;
	for (int i = 0; i < max_conn + 1; i++) {
		ieee80211_bss_st *bss = conn->ni_ap_bss[i];
		if ((bss != NULL) && memcmp(bssid,bss->ni_bssid,6) == 0) {
			return bss;
		}
	}
	return NULL;
}


void ICACHE_FLASH_ATTR
cnx_node_remove(ieee80211_bss_st *bss)
{
	ieee80211_conn_st *conn = g_ic.ic_if1_conn;

	int max_conn = g_ic.ic_profile.softap.max_connection;
	for (int i = 1; i < max_conn + 1; i++) {
		if (conn->ni_ap_bss[i] != bss) {
			continue;
		}
		wDev_remove_KeyEntry(bss->ni_ptk_index + 2);
		wpa_auth_sta_deinit(bss->ni_wpa_sm);
		os_free(g_ic.ic_key[bss->ni_ptk_index]);
		g_ic.ic_key[bss->ni_ptk_index] = NULL;
		pwrsave_flushq(bss,1);
		os_free(bss);
		conn->ni_ap_bss[i] = NULL;
	}
}


ieee80211_phytype_t ieee80211_phy_type_get(ieee80211_bss_st *bss);
void ic_set_sta(uint8_t ifidx,uint8_t set,uint8_t *mac,uint8_t index,uint16_t aid,ieee80211_phytype_t phymode,uint8_t ampdu_factor, uint8_t ampdu_density);

void ICACHE_FLASH_ATTR
cnx_node_leave(ieee80211_conn_st *ni,ieee80211_bss_st *bss)
{
	System_Event_st *param;
	
	ets_timer_disarm(&bss->ni_inactivity_timer);
	if (ni->ni_ap_bss[0] != bss) {
		if ((bss->ni_associd & 0x3fff) == 0) {
			os_printf_plus("max connection!\n");
		}
		else {
			os_printf_plus("station: %02x:%02x:%02x:%02x:%02x:%02x leave, AID = %d\n");
		}
		if (bss->ni_associd != 0) {
			if ((event_cb != NULL) && (param = os_zalloc(sizeof(*param))), param != NULL) {
				param->event = EVENT_SOFTAPMODE_STADISCONNECTED;
				memcpy(param->event_info.sta_disconnected.mac,bss,6);
				param->event_info.sta_disconnected.aid = bss->ni_associd;
				if (ets_post(PRIO_EVENT,EVENT_SOFTAPMODE_STADISCONNECTED,(ETSParam)param) != OK) {
					os_free(param);
				}
			}
			ieee80211_phytype_t phymode = ieee80211_phy_type_get(bss);
			int aid = bss->ni_associd & 0xfff;
			ic_set_sta(1,0,bss->ni_bssid,aid,aid,phymode,0,0);
			g_ic.ic_aid_bitmap &= ~(1 << (bss->ni_associd & 0xff));
			bss->ni_associd = 0;
		}
		cnx_node_remove(bss);
	}
	return;
}

void ICACHE_FLASH_ATTR
send_ap_probe(void)
{
	struct netif *netif = (g_ic.ic_if0_conn)->ni_ifp;
	ieee80211_bss_st *bss = (g_ic.ic_if0_conn)->ni_bss;

	if (((netif == NULL) || (bss == NULL)) || (netif == (struct netif*)0xffffffcd)) {
		(g_ic.ic_if0_conn)->mgd_probe_send_count = '\x05';
	}
	else if ((g_ic.ic_if0_conn)->mgd_probe_send_count < 3) {
		ieee80211_send_probereq(bss,netif->hwaddr,bss->ni_bssid,bss->ni_bssid,g_ic.ic_profile.sta.ssid.ssid,
					g_ic.ic_profile.sta.ssid.len,'\0');
	}
	else {
		ieee80211_send_probereq(bss,netif->hwaddr,ethbroadcast.addr,ethbroadcast.addr,g_ic.ic_profile.sta.ssid.ssid,
					g_ic.ic_profile.sta.ssid.len,'\0');
	}
	(g_ic.ic_if0_conn)->mgd_probe_send_count++;
	ets_timer_disarm(&(g_ic.ic_if0_conn)->ni_beacon_timeout);
	ets_timer_arm_new(&(g_ic.ic_if0_conn)->ni_beacon_timeout,500,false,true);
}

void cnx_beacon_timeout(void *unused_arg);

void ICACHE_FLASH_ATTR
mgd_probe_send_timeout(void *unused_arg)
{
	ETSTimer *beacon_timeout = &(g_ic.ic_if0_conn)->ni_beacon_timeout;
	if ((g_ic.ic_if0_conn)->mgd_probe_send_count == 0) {
		ets_timer_setfn(beacon_timeout,cnx_beacon_timeout,NULL);
		ets_timer_disarm(&(g_ic.ic_if0_conn)->ni_beacon_timeout);
		ets_timer_arm_new(&(g_ic.ic_if0_conn)->ni_beacon_timeout,(g_ic.ic_if0_conn)->ni_bss->ni_intval * 0x1e,false,true);
	}
	else if ((g_ic.ic_if0_conn)->mgd_probe_send_count < 5) {
		ets_timer_setfn(beacon_timeout,mgd_probe_send_timeout,NULL);
		send_ap_probe();
	}
	else {
		ets_timer_setfn(beacon_timeout,cnx_beacon_timeout,NULL);
		os_printf_plus("ap_probe_send over, rest wifi status to disassoc\n");
		ieee80211_sta_new_state(&g_ic,IEEE80211_S_INIT,1);
		if ((gScanStruct.ss_state & 1) == 0) {
			(g_ic.ic_if0_conn)->ni_connect_step = '\0';
			(g_ic.ic_if0_conn)->ni_connect_status = '\0';
			(g_ic.ic_if0_conn)->ni_connect_err_time = '\0';
			if (wifi_station_get_reconnect_policy()) {
				cnx_sta_connect_cmd(&g_ic.ic_profile,'\0');
			}
		}
		else {
			(g_ic.ic_if0_conn)->ni_connect_step = '\x03';
			(g_ic.ic_if0_conn)->ni_connect_status = '\x03';
			(g_ic.ic_if0_conn)->ni_connect_err_time = '\0';
		}
	}
}

void ICACHE_FLASH_ATTR
cnx_beacon_timeout(void *unused_arg)
{
	os_printf_plus("bcn_timout,ap_probe_send_start\n");
	(g_ic.ic_if0_conn)->mgd_probe_send_count = '\0';
	ets_timer_setfn(&(g_ic.ic_if0_conn)->ni_beacon_timeout,mgd_probe_send_timeout,NULL);
	send_ap_probe();
}

void ICACHE_FLASH_ATTR
cnx_handshake_timeout(void *unused_arg)
{
	bool bVar1;
	
	if (g_ic.ic_mode == '\x04') {
		g_ic.ic_mode = '\0';
		if ((g_ic.ic_wpa2 != NULL) && ((g_ic.ic_wpa2)->wpa2_deinit != NULL)) {
			g_ic.ic_wpa2->wpa2_deinit();
		}
	}
	ieee80211_sta_new_state(&g_ic,IEEE80211_S_INIT,2);
	if (wifi_station_get_reconnect_policy()) {
		cnx_connect_timeout();
	}
}

void pm_keep_active_enable(void);
void pm_keep_active_disable(void);
err_t dhcp_release(struct netif *netif);
void dhcp_stop(struct netif *netif);
void dhcp_cleanup(struct netif *netif);
void wDev_remove_KeyEntry_all_cnx(uint8_t ifidx);

void ICACHE_FLASH_ATTR
cnx_sta_associated(ieee80211_conn_st *conn,ieee80211_bss_st *bss,bool isAssoc)
{
	ieee80211_phytype_t phymode = ieee80211_phy_type_get(bss);
	ic_set_sta('\0','\x01',bss->ni_bssid,'\0',bss->ni_associd & 0xfff,phymode,'\0','\0');
	pm_keep_active_enable();
	RTCMEM->SYSTEM_CHANCFG = bss->ni_channel->ch_ieee | 0x10000;
	ets_timer_disarm(&conn->ni_beacon_timeout);
	ets_timer_setfn(&conn->ni_beacon_timeout,cnx_beacon_timeout,NULL);
	ETSTimer *connect_timeout = &conn->ni_connect_timeout;
	ets_timer_disarm(connect_timeout);
	ets_timer_setfn(connect_timeout,cnx_handshake_timeout,NULL);
	if ((g_ic.ic_profile.wps_type == WPS_TYPE_DISABLE) ||
		 (g_ic.ic_profile.wps_status != WPS_STATUS_PENDING)) {
		if (g_ic.ic_profile.sta.authmode == '\x04') {
			if ((g_ic.ic_wpa2 != NULL) && ((g_ic.ic_wpa2)->wpa2_init != NULL)) {
				if (g_ic.ic_wpa2->wpa2_init() == 0) {
					ets_timer_arm_new(connect_timeout,30000,false,true);
					if ((g_ic.ic_wpa2 != NULL) && ((g_ic.ic_wpa2)->wpa2_start != NULL)) {
						g_ic.ic_wpa2->wpa2_start();
					}
				}
				else {
					ets_timer_arm_new(connect_timeout,1000,false,true);
				}
			}
		}
		else {
			ets_timer_arm_new(connect_timeout,10000,false,true);
		}
	}
	else {
		ets_timer_arm_new(connect_timeout,40000,false,true);
		if ((g_ic.ic_wps != NULL) && ((g_ic.ic_wps)->wps_start_pending != NULL))
		{
			g_ic.ic_wps->wps_start_pending();
		}
	}
}

void ICACHE_FLASH_ATTR
cnx_sta_leave(ieee80211_conn_st *conn,ieee80211_bss_st *bss)
{
	struct netif *netif = conn->ni_ifp;

	pm_keep_active_disable();
	if (bss->ni_associd != 0) {
		ieee80211_phytype_t phymode = ieee80211_phy_type_get(bss);
		ic_set_sta('\0','\0',bss->ni_bssid,'\0',bss->ni_associd & 0xfff,phymode,'\0','\0');
	}
	netif_set_down(netif);
	if ((netif->flags >> 3 & 1) != 0) {
		dhcp_release(netif);
		dhcp_stop(netif);
		dhcp_cleanup(netif);
	}
	(netif->ip_addr).addr = 0;
	(netif->netmask).addr = 0;
	(netif->gw).addr = 0;
	wDev_remove_KeyEntry_all_cnx('\0');
	os_free(g_ic.ic_key[bss->ni_ptk_index]);
	g_ic.ic_key[bss->ni_ptk_index] = NULL;
	if (g_ic.ic_key[0] != NULL) {
		os_free(g_ic.ic_key[0]);
		g_ic.ic_key[0] = NULL;
	}
	if (g_ic.ic_key[1] != NULL) {
		os_free(g_ic.ic_key[1]);
		g_ic.ic_key[1] = NULL;
	}
	wDev_SetRxPolicy(RX_DISABLE,'\0',NULL);
	g_cnxMgr.cnx_state = CNX_S_DISCONNECTED;
	if ((bss->bss_flags >> 1 & 1) != 0) {
		bss->bss_flags = bss->bss_flags & 0xfd;
		cnx_rc_update_state_metric(bss,-7,METRIC_UPDATE);
		g_cnxMgr.cnx_last_bss = bss;
	}
	cnx_remove_rc(bss);
	g_cnxMgr.cnx_cur_bss = NULL;
	bss->ni_flags = bss->ni_flags & 0xfffffffe;
	conn->ni_bss = NULL;
}


int ieee80211_send_mgmt(ieee80211_conn_st *conn, int type, int arg);
void wpa_auth_sta_deinit(struct wpa_state_machine *sm);
struct wpa_state_machine *wpa_auth_sta_init(wpa_authenticator_st *wpa_auth, uint8_t *addr);
int wpa_validate_wpa_ie(wpa_authenticator_st *wpa_auth, struct wpa_state_machine *sm, uint8_t *wpa_ie, uint32_t wpa_ie_len);
int wpa_auth_sta_associated(wpa_authenticator_st *wpa_auth, struct wpa_state_machine *sm);

void ICACHE_FLASH_ATTR
cnx_node_join(ieee80211_conn_st *ni,ieee80211_bss_st *bss,int resp)
{
	if (memcmp(bss,&ethbroadcast,6) == 0) {
		ieee80211_send_mgmt(ni,resp,IEEE80211_STATUS_TOOMANY);
		os_printf_plus("bss == broadcast (max connection!)\n");
		cnx_node_leave(ni,bss);
		return;
	}

	int aid = bss->ni_associd;
	if (aid) {
		if ((g_ic.ic_aid_bitmap >> aid) & 1) {
			os_printf_plus("err already associed!\n");
			ieee80211_send_mgmt(ni,resp,IEEE80211_STATUS_TOOMANY);
			cnx_node_leave(ni,bss);
			return;
		}
	} else {
		int max_conn = g_ic.ic_profile.softap.max_connection;
		int i;
		for (i = 1; i < max_conn + 1; i++) {
			if ((g_ic.ic_aid_bitmap & (1 << i)) == 0) break;
		}
		if (max_conn < i) {
			ieee80211_send_mgmt(ni,resp,IEEE80211_STATUS_TOOMANY);
			cnx_node_leave(ni,bss);
			return;
		}
		bss->ni_associd = i | 0xc000;
		g_ic.ic_aid_bitmap = g_ic.ic_aid_bitmap | (1 << i);
		ieee80211_phytype_t phymode = ieee80211_phy_type_get(bss);
		ic_set_sta(1,1,bss->ni_bssid,i,i,phymode,0,0);
		aid = bss->ni_associd;
	}

	os_printf_plus("station: %02x:%02x:%02x:%02x:%02x:%02x join, AID = %d\n",
			 bss->ni_bssid[0],bss->ni_bssid[1],bss->ni_bssid[2],
			 bss->ni_bssid[3],bss->ni_bssid[4],bss->ni_bssid[5],
			 aid & 0x3fff);

	join_deny_flag |= 1;
	if (event_cb != NULL) {
		System_Event_st *sev = os_zalloc(sizeof(*sev));
		if (sev != NULL) {
			sev->event = EVENT_SOFTAPMODE_STACONNECTED;
			Event_SoftAPMode_StaConnected_st *info = &sev->event_info.sta_connected;
			memcpy(info->mac,bss,6);

			(sev->event_info).connected.ssid[6] = (uint8)bss->ni_associd;
			if ((join_deny_flag & 4) == 0) {
				event_cb(sev);
				os_free(sev);
			}
			else {
				if (ets_post(0x15,5,(ETSParam)sev) != OK) {
					os_free(sev);
				}
			}
		}
	}

	int saved_join_deny = join_deny_flag;
	join_deny_flag &= 4;
	if ((saved_join_deny & 2) == 1) {
		ieee80211_send_mgmt(ni,resp,IEEE80211_STATUS_TOOMANY);
		cnx_node_leave(ni,bss);
		return;
	}

	memcpy(ni->ni_bss,bss,6);
	ni->ni_bss->ni_associd = bss->ni_associd;

	ieee80211_send_mgmt(ni,resp,0);

	memset(ni->ni_bss,0xff,6);
	ni->ni_bss->ni_associd = 0;

	hostapd_data_st *hpad = ni->ni_hapd;
	if (hpad != NULL) {
		wpa_authenticator_st *wpa_auth = hpad->wpa_auth;
		if ((wpa_auth->conf).wpa != 0) {
			wpa_auth_sta_deinit(bss->ni_wpa_sm);
			struct wpa_state_machine *sm = wpa_auth_sta_init(ni->ni_hapd->wpa_auth,bss->ni_bssid);
			bss->ni_wpa_sm = sm;
			if (sm == NULL) {
				return;
			}
			if (wpa_validate_wpa_ie(ni->ni_hapd->wpa_auth,sm,bss->ni_wpa_ie,
						bss->ni_wpa_ie_len) != 0) {
				return;
			}
			wpa_auth = ni->ni_hapd->wpa_auth;
		}
		wpa_auth_sta_associated(wpa_auth,bss->ni_wpa_sm);
		bss->ni_datapolicy = 1;
	}
}


STATUS ppRegisterTxCallback(pp_tx_cb_t fn,uint8_t id);

void ICACHE_FLASH_ATTR
cnx_attach(ieee80211com_st *ic)
{
	bzero(&g_cnxMgr,sizeof(g_cnxMgr));
	ic->cnxmgr = &g_cnxMgr;
	g_cnxMgr.cnx_rc_weight = 15;
	g_cnxMgr.cnx_state = CNX_S_DISCONNECTED;
	ppRegisterTxCallback(cnx_probe_rc_tx_cb,0);
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

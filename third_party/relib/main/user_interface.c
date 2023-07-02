#include <assert.h>
#include <stdint.h>

#include "c_types.h"
#include "mem.h"
#include "osapi.h"
#include "spi_flash.h"

#include "relib/relib.h"
#include "relib/esp8266.h"
#include "relib/xtensa.h"
#include "relib/ets_rom.h"

#include "relib/eagle_lwip_if.h"

#include "relib/s/ieee80211com.h"
#include "relib/s/wl_profile.h"
#include "relib/s/wifi_flash_header.h"
#include "relib/s/if_param.h"
#include "relib/s/system_event.h"
#include "relib/s/station_config.h"
#include "relib/s/softap_config.h"
#include "relib/s/rst_info.h"

extern ieee80211com_st g_ic; /* defined in ieee80211.c */
extern if_param_st info; /* defined in app_main.c */
extern ETSTimer sta_con_timer;
extern bool user_init_flag;
extern bool BcnEb_update;
extern char *hostname;

typedef void (*init_done_cb_t)(void);
typedef void (*wifi_event_handler_cb_t)(System_Event_st *event);
typedef void (*wifi_promiscuous_cb_t)(uint8_t *buf, uint16_t len);

init_done_cb_t done_cb;
wifi_event_handler_cb_t event_cb;
wifi_promiscuous_cb_t promiscuous_cb;
int system_param_sector_start;
bool timer2_ms_flag;
bool dhcps_flag;
bool dhcpc_flag;
bool deep_sleep_flag;
uint8_t default_interface;
uint8_t status_led_output_level;
rst_info_st rst_if;

void gpio_output_set(uint32_t set_mask, uint32_t clear_mask, uint32_t enable_mask, uint32_t disable_mask);

void ICACHE_FLASH_ATTR
system_init_done_cb(init_done_cb_t cb)
{
	done_cb = cb;
}

int
os_printf_plus(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	int ret = ets_vprintf(ets_write_char, format, ap);
	va_end(ap);
	return ret;
}


void ppRecycleRxPkt(void *pkt);

void
system_pp_recycle_rx_pkt(void *pkt)
{
	ppRecycleRxPkt(pkt);
}

void ICACHE_FLASH_ATTR
system_restart_local(void)
{
	os_printf_plus("system_restart_local not implemented\n");
	while (1);
}

uint8_t ets_get_cpu_frequency(void);
void ets_update_cpu_frequency(uint8_t freq);

bool ICACHE_FLASH_ATTR
system_update_cpu_freq(uint8_t freq)
{
	switch (freq) {
	case 80: DPORT->CTL &= ~1; break;
	case 160: DPORT->CTL |= 1; break;
	default: return false;
	}
	ets_update_cpu_frequency(freq);
	return true;
}


uint8_t ICACHE_FLASH_ATTR
system_get_cpu_freq(void)
{
	return ets_get_cpu_frequency();
}

bool cpu_overclock;

bool ICACHE_FLASH_ATTR
system_overclock(void)
{
	if (system_get_cpu_freq() > 80) {
		return false;
	}
	cpu_overclock = true;
	return system_update_cpu_freq(160);
}

bool ICACHE_FLASH_ATTR
system_restoreclock(void)
{
	if ((system_get_cpu_freq() != 160) || (cpu_overclock == false)) {
		return false;
	}
	cpu_overclock = false;
	system_update_cpu_freq(80);
	return true;
}

bool ICACHE_FLASH_ATTR
system_get_string_from_flash(void *flash_str,void *ram_str,uint8_t ram_str_len)
{
	if (((uint32_t)flash_str & 3) == 0) {
		int len = strlen((char *)flash_str) + 1;
		if (len <= ram_str_len) {
			memcpy(ram_str, flash_str, len);
			return true;
		}
	}
	return false;
}

bool
system_rtc_mem_read(uint8_t src_addr,void *des_addr,uint16_t load_size)
{
	/* Note: Due to the length check here, this includes the SYSTEM and USER regions */
	if (src_addr >= (0x300 / 4) || des_addr == NULL || ((uint32_t)des_addr & 3) != 0 || (load_size + src_addr > 0x300)) {
		return false;
	}
	load_size = (load_size + 3) / 4;
	uint32_t *dst = des_addr;
	for (int i = 0; i < load_size; i++) {
		*dst++ = RTCMEM->SYSTEM[src_addr++];
	}
	return true;
}

bool
system_rtc_mem_write(uint8_t des_addr,void *src_addr,uint16_t save_size)
{
	/* Note: Due to the length check here, this includes the SYSTEM and USER regions */
	if (des_addr >= (0x300 / 4) || src_addr == NULL || ((uint32_t)src_addr & 3) != 0 || (save_size + des_addr > 0x300)) {
		return false;
	}
	save_size = (save_size + 3) / 4;
	uint32_t *src = src_addr;
	for (int i = 0; i < save_size; i++) {
		RTCMEM->SYSTEM[des_addr++] = *src++;
	}
	return true;
}

static ETSTimer SleepDeferTimer;
static bool SleepFuncRegisterIni;
static bool SleepDeferTimerRun;
static uint8_t DeferFuncIndexArr[10];
static uint8_t DeferFuncNum;
static uint8_t CurDeferFuncIndex;
static uint8_t SleepDeferCurrent3;
static uint8_t SleepDeferCurrent5;
static uint8_t SleepDeferCurrent6;
static uint8_t SleepDeferOpmode;
static station_config_st SleepDeferStationConfig;
static softap_config_st SleepDeferSoftAPConfig;
static bool OpmodChgIsOnGoing;

bool fpm_allow_tx(void);
void fpm_do_wakeup(void);
bool pm_is_open(void);
bool pm_is_waked(void);
void pm_post(int);
bool wifi_station_disconnect(void);
bool wifi_station_connect(void);
bool wifi_station_set_config_local(station_config_st *config, bool current);
void system_restart_local(void);
bool wifi_set_opmode_local(uint8_t new_opemode, uint8_t current_opmode);
bool wifi_softap_set_config_local(softap_config_st *config, bool current);

enum sleep_fn_idx {
	SLEEP_DEFER_NONE = 0,
	SLEEP_DEFER_DISCONNECT = 1,
	SLEEP_DEFER_CONNECT = 2,
	SLEEP_DEFER_STATION_SET_CONFIG = 3,
	SLEEP_DEFER_SYSTEM_RESTART = 4,
	SLEEP_DEFER_SET_OPMODE = 5,
	SLEEP_DEFER_SOFTAP_SET_CONFIG = 6,
} __attribute__((packed));
typedef enum sleep_fn_idx sleep_fn_idx_t;

void ICACHE_FLASH_ATTR
SleepDeferTimeOut(void *arg)
{
	bool is_waked;
	bool is_open;
	int curIdx;
	bool run;
	uint8_t fnIdx;
	
	run = false;
	SleepDeferTimerRun = false;
	if (DeferFuncNum != 0) {
		do {
			is_waked = pm_is_waked();
			if ((!is_waked) && (is_open = pm_is_open(), is_open)) break;
			curIdx = CurDeferFuncIndex;
			fnIdx = DeferFuncIndexArr[curIdx];
			if (fnIdx == 1) {
				wifi_station_disconnect();
LAB_4023b8c4:
				curIdx = CurDeferFuncIndex;
			}
			else {
				if (fnIdx == 2) {
					wifi_station_connect();
					goto LAB_4023b8c4;
				}
				if (fnIdx == 3) {
					wifi_station_set_config_local(&SleepDeferStationConfig,SleepDeferCurrent3);
					goto LAB_4023b8c4;
				}
				if (fnIdx == 4) {
					system_restart_local();
					goto LAB_4023b8c4;
				}
				if (fnIdx == 5) {
					wifi_set_opmode_local(SleepDeferOpmode,SleepDeferCurrent5);
					goto LAB_4023b8c4;
				}
				if (fnIdx == 6) {
					wifi_softap_set_config_local(&SleepDeferSoftAPConfig,SleepDeferCurrent6);
					goto LAB_4023b8c4;
				}
			}
			curIdx = curIdx + 1 & 0xff;
			DeferFuncNum = DeferFuncNum + 0xff;
			if (curIdx == 10) {
				CurDeferFuncIndex = 0;
			}
			else {
				CurDeferFuncIndex = curIdx;
			}
		} while (DeferFuncNum != 0);
	}
	if (DeferFuncNum == 0) {
		ets_timer_disarm(&SleepDeferTimer);
	}
	else {
		is_waked = fpm_allow_tx();
		if (!is_waked) {
			fpm_do_wakeup();
		}
		pm_post(1);
		ets_timer_disarm(&SleepDeferTimer);
		ets_timer_arm_new(&SleepDeferTimer,10,false,true);
		run = true;
	}
	SleepDeferTimerRun = run;
}

int ICACHE_FLASH_ATTR
CheckSleep(sleep_fn_idx_t func_idx)
{
	uint32_t newFnIdx;
	if (!fpm_allow_tx()) {
		fpm_do_wakeup();
	}
	if (pm_is_open()) {
		if (SleepFuncRegisterIni == '\0') {
			ets_timer_setfn(&SleepDeferTimer,SleepDeferTimeOut,(void *)0x0);
			SleepFuncRegisterIni = '\x01';
		}
		if ((!pm_is_waked()) || (SleepDeferTimerRun == '\x01')) {
			if (SleepDeferTimerRun == '\0') {
				pm_post(1);
				ets_timer_disarm(&SleepDeferTimer);
				ets_timer_arm_new(&SleepDeferTimer,10,false,true);
				SleepDeferTimerRun = '\x01';
			}
			newFnIdx = DeferFuncNum++;
			if (newFnIdx > 10) {
				os_printf_plus("DEFERRED FUNC NUMBER IS BIGGER THAN 10\n");
				newFnIdx = 10;
				DeferFuncNum = 10;
			}
			int idx = (CurDeferFuncIndex + newFnIdx - 1) % 10;
			DeferFuncIndexArr[idx] = func_idx;
			return -1;
		}
	}
	return 0;
}

bool ICACHE_FLASH_ATTR
system_param_load(uint16_t start_sec, uint16_t offset, void *param, uint16 len)
{
	wifi_flash_header_st fhead;

	if (param == NULL || (len + offset) > flashchip->sector_size) {
		return false;
	}

	spi_flash_read((start_sec + 2)*flashchip->sector_size, (uint32_t*)&fhead, sizeof(fhead));

	uint32_t data_sec = start_sec + !!fhead.bank;
	spi_flash_read(data_sec * flashchip->sector_size + offset, param, len);
	return true;
}

void ICACHE_FLASH_ATTR
wifi_param_save_protect_with_check(uint16_t startsector, int sectorsize, void *pdata, uint16_t len)
{
#if 1
	os_printf_plus("Unimpl: wifi_param_save_protect_with_check\n");
#else
	uint32_t *dst_addr;
	SpiFlashOpResult SVar1;
	int iVar2;
	uint size;
	
	size = (uint)param_4;
	dst_addr = (uint32_t *)pvPortZalloc(size,"user_interface.c",0x802);
	if (dst_addr == (uint32_t *)0x0) {
		return;
	}
	do {
		SVar1 = spi_flash_erase_sector(param_1);
		if (SVar1 == SPI_FLASH_RESULT_OK) {
			SVar1 = spi_flash_write((uint)param_1 * param_2,(uint32 *)param_3,size);
			if (SVar1 == SPI_FLASH_RESULT_OK) {
				SVar1 = spi_flash_read((uint)param_1 * param_2,dst_addr,size);
				if (SVar1 == SPI_FLASH_RESULT_OK) {
					iVar2 = memcmp(dst_addr,param_3,size);
					if (iVar2 == 0) {
						vPortFree(dst_addr,"user_interface.c",0x81d);
						return;
					}
				}
				else {
					os_printf_plus((char *)&flash_str$8889_194_11);
				}
			}
			else {
				os_printf_plus((char *)&flash_str$8888_194_10);
			}
		}
		else {
			os_printf_plus((char *)&flash_str$8886_194_9);
		}
		os_printf_plus("sec %x error\n");
	} while( true );
#endif
}

bool ICACHE_FLASH_ATTR
system_param_save_with_protect(uint16_t start_sec, void *param, uint16 len)
{
	os_printf_plus("Unimpl: system_param_save_with_protect\n");
}

void ICACHE_FLASH_ATTR
system_save_sys_param(void)
{
	system_param_save_with_protect(system_param_sector_start,&g_ic.ic_profile,sizeof(g_ic.ic_profile));
}


bool wifi_station_get_config_local(station_config_st *config, uint8_t current)
{
	wl_profile_st *param;
	uint8 *password;
	
	if (config == NULL) {
		return false;
	}

	if (current == '\x01') {
		param = &g_ic.ic_profile;
	}
	else {
		param = os_malloc(sizeof(*param));
		if (param == NULL) {
			return false;
		}
		system_param_load(system_param_sector_start,0,param,sizeof(*param));
	}
	if ((param->sta).ssid.len == -1) {
		bzero(&param->sta.ssid,sizeof(param->sta.ssid));
		bzero(&param->sta.password,sizeof(param->sta.password));
	}
	memcpy(config->ssid,(param->sta).ssid.ssid,sizeof(config->ssid));
	memcpy(config->password,param->sta.password,sizeof(config->password));
	config->bssid_set = !!(param->sta).bssid_set;
	memcpy(config->bssid,(param->sta).bssid,sizeof(config->bssid));
	config->open_and_wep_mode_disable = param->sta_open_and_wep_mode_disable == '\x01';
	if (current != '\x01') {
		os_free(param);
	}
	config->channel = (param->sta).channel;
	return true;
}

bool ICACHE_FLASH_ATTR
wifi_station_get_config(station_config_st *config)
{
	return wifi_station_get_config_local(config,'\x01');
}

static bool PmkCurrent;

extern uint8_t no_ap_found_index;

void ICACHE_FLASH_ATTR
wifi_station_set_config_local_3(void)
{
	wl_profile_st *param = os_malloc(sizeof(*param));
	if (param == NULL) {
		return;
	}
	system_param_load(system_param_sector_start,0,param,sizeof(*param));
	memset(&param->sta,0xff,sizeof(param->sta));
	memset(&g_ic.ic_profile.sta,0xff,sizeof(g_ic.ic_profile.sta));
	if (g_ic.ic_profile.sta.ssid.len == -1) {
		bzero(&g_ic.ic_profile.sta.ssid,sizeof(g_ic.ic_profile.sta.ssid));
		bzero(g_ic.ic_profile.sta.password,sizeof(g_ic.ic_profile.sta.password));
	}
	if (true < g_ic.ic_profile.sta.bssid_set) {
		g_ic.ic_profile.sta.bssid_set = false;
	}
	if ((param->sta).ssid.len == -1) {
		bzero(&param->sta.ssid,sizeof(param->sta.ssid));
		bzero((param->sta).password,sizeof(param->sta.password));
	}
	if (true < (param->sta).bssid_set) {
		(param->sta).bssid_set = false;
	}
	param->sta_open_and_wep_mode_disable = '\0';
	system_param_save_with_protect(system_param_sector_start,param,sizeof(*param));
	os_free(param);
}

void ICACHE_FLASH_ATTR
wifi_station_set_config_local_2(wl_profile_st *profile,station_config_st *config,uint8_t ap_index,uint8_t current)
{
	bool bVar1;
	size_t sVar2;
	int8 iVar3;
	size_t sVar4;
	int iVar5;
	
	(profile->ap_change_param).ap_index = ap_index;
	sVar4 = strlen((char *)config);
	sVar2 = 0x20;
	if (sVar4 < 0x21) {
		sVar2 = sVar4;
	}
	profile->save_ap_info[ap_index].ssid.len = sVar2;
	memcpy(profile->save_ap_info[ap_index].ssid.ssid,config,0x20);
	memcpy(profile->save_ap_info[ap_index].password,config->password,0x40);
	iVar5 = memcmp(profile->sta.ssid.ssid,config->ssid,0x20);
	if ((iVar5 == 0) && (iVar5 = memcmp(profile->sta.password,config->password,0x40), iVar5 == 0)) {
		if (profile->pmk_info[ap_index].pmk[0] == 0xff) {
			(profile->sta).reset_param = '\x01';
		}
		else {
			memcpy((profile->sta).pmk,profile->pmk_info + ap_index,0x20);
			(profile->sta).reset_param = '\0';
		}
	}
	else {
		memset(profile->pmk_info + ap_index,0xff,0x20);
		(profile->sta).reset_param = '\x01';
	}
	(profile->sta).ssid.len = profile->save_ap_info[ap_index].ssid.len;
	memcpy(profile->sta.ssid.ssid,config->ssid,0x20);
	memcpy(profile->sta.password,config->password,0x40);
	(profile->sta).password[0x40] = '\0';
	RTCMEM->SYSTEM_CHANCFG = 0x10000;
	if (config->bssid_set == '\x01') {
		(profile->sta).bssid_set = true;
		profile->save_ap_info_2[ap_index].bssid_set = true;
		memcpy((profile->sta).bssid,config->bssid,6);
		memcpy(profile->save_ap_info_2[ap_index].bssid,config->bssid,6);
	}
	else {
		(profile->sta).bssid_set = false;
		profile->save_ap_info_2[ap_index].bssid_set = false;
	}
	iVar3 = (config->threshold).rssi;
	if (-1 < iVar3) {
		iVar3 = -0x7f;
	}
	profile->minimum_rssi_in_fast_scan = iVar3;
	auth_mode_t authmode = (config->threshold).authmode;
	if (AUTH_WPA2_PSK < authmode) {
		authmode = AUTH_OPEN;
	}
	profile->minimum_auth_mode_in_fast_scan = authmode;
	bVar1 = config->open_and_wep_mode_disable;
	profile->all_channel_scan = config->all_channel_scan;
	profile->sta_open_and_wep_mode_disable = bVar1 == true;
	if (current == '\x01') {
		system_param_save_with_protect((uint16)system_param_sector_start,profile,sizeof(*profile));
	}
	(profile->sta).channel = config->channel;
	return;
}

void ICACHE_FLASH_ATTR
wifi_station_set_config_local_1(wl_profile_st *profile,station_config_st *config,uint8_t current)
{
	static uint8_t ap_id;
	int8 iVar1;
	bool bVar2;
	int iVar3;
	int ap_idx;
	int ap_num;
	
	ap_idx = 0;
	ap_num = (profile->ap_change_param).ap_number;
	do {
		if ((ap_num <= ap_idx) || (4 < ap_idx)) {
			if (ap_num != 0) {
				ap_idx = 0;
				do {
					if (profile->save_ap_info[ap_idx].ssid.ssid[0] == 0xff) {
						wifi_station_set_config_local_2(profile,config,(uint8)ap_idx,current);
						return;
					}
					ap_idx = ap_idx + 1 & 0xff;
				} while (ap_num != ap_idx);
			}
			if (ap_num == ap_id) {
				ap_id = 0;
			}
			wifi_station_set_config_local_2(profile,config,ap_id,current);
			ap_id = ap_id + 1;
			return;
		}
		iVar3 = memcmp(profile->save_ap_info[ap_idx].ssid.ssid,config,0x20);
		if ((iVar3 == 0) &&
			 (iVar3 = memcmp(profile->save_ap_info[ap_idx].password,config->password,0x40),
			 iVar3 == 0)) {
			if (config->bssid_set == '\x01') {
				iVar3 = memcmp(profile->save_ap_info_2[ap_idx].bssid,config->bssid,6);
				bVar2 = iVar3 == 0;
			}
			else {
				bVar2 = true;
			}
			if (bVar2) {
				iVar1 = (config->threshold).rssi;
				if (iVar1 < 0) {
					bVar2 = profile->minimum_rssi_in_fast_scan == iVar1;
				}
				else {
					bVar2 = true;
				}
				if ((((bVar2) &&
						 (profile->minimum_auth_mode_in_fast_scan == (config->threshold).authmode)) &&
						(config->open_and_wep_mode_disable == (bool)profile->sta_open_and_wep_mode_disable)) &&
					 ((config->all_channel_scan == profile->all_channel_scan &&
						((profile->save_ap_info_2[ap_idx].bssid_set != true || (config->bssid_set == '\x01'))))))
				{
					if ((profile->ap_change_param).ap_index == ap_idx) {
						return;
					}
					wifi_station_set_config_local_2(profile,config,(uint8)ap_idx,current);
					return;
				}
			}
		}
		ap_idx = ap_idx + 1 & 0xff;
	} while( true );
}


uint8_t wifi_get_opmode(void);

bool ICACHE_FLASH_ATTR
wifi_station_set_config_local(station_config_st *config, uint8_t current)
{
	wl_profile_st *profile;
	
	uint8_t opmode = wifi_get_opmode();
	if (((config == NULL) || (opmode == '\x02')) || (opmode == '\0')) {
		return false;
	}

	if (current == '\x02') {
		wifi_station_set_config_local_3();
	}
	else {
		no_ap_found_index = '\0';
		if (CheckSleep(SLEEP_DEFER_STATION_SET_CONFIG) == -1) {
			memcpy(&SleepDeferStationConfig,config,sizeof(SleepDeferStationConfig));
			SleepDeferCurrent3 = current;
		}
		else {
			if (current == '\x01') {
				profile = os_malloc(sizeof(*profile));
				if (profile == NULL) {
					return false;
				}
				system_param_load(system_param_sector_start,0,profile,sizeof(*profile));
				wifi_station_set_config_local_1(profile,config,'\x01');
				os_free(profile);
			}
			wifi_station_set_config_local_1(&g_ic.ic_profile,config,'\0');
		}
	}
	return true;
}


bool ICACHE_FLASH_ATTR
wifi_station_set_config(station_config_st *config)
{
	PmkCurrent = '\x01';
	return wifi_station_set_config_local(config,'\x01');
}

bool ICACHE_FLASH_ATTR
wifi_station_set_config_current(station_config_st *config)
{
	PmkCurrent = '\0';
	return wifi_station_set_config_local(config,'\0');
}

void ICACHE_FLASH_ATTR
wifi_station_save_bssid(void)
{
	wl_profile_st *param;
	uint8_t *bssid;
	int iVar1;

	if (PmkCurrent) {
		param = os_malloc(sizeof(*param));
		if (param != NULL) {
			system_param_load((uint16)system_param_sector_start,0,param,sizeof(*param));
			bssid = (param->sta).bssid;
			if (memcmp(bssid,g_ic.ic_profile.sta.bssid,6) != 0) {
				memcpy(bssid,g_ic.ic_profile.sta.bssid,6);
				system_param_save_with_protect((uint16)system_param_sector_start,param,sizeof(*param));
			}
			os_free(param);
		}
	}
}

void ICACHE_FLASH_ATTR
wifi_station_save_ap_channel(uint8_t channel)
{
	int iVar1;
	wl_profile_st *param;
	uint32_t uVar2;
	uint32_t uVar3;

	uVar3 = (uint32_t)g_ic.ic_profile.ap_change_param.ap_number;
	g_ic.ic_profile.sta.channel = channel;
	if (uVar3 == 0) {
		uVar2 = 0;
	}
	else {
		uVar2 = 0;
		do {
			iVar1 = memcmp(g_ic.ic_profile.save_ap_info[uVar2].ssid.ssid,
					g_ic.ic_profile.sta.ssid.ssid,sizeof(g_ic.ic_profile.sta.ssid.ssid));
			if ((iVar1 == 0) &&
				 (iVar1 = memcmp(g_ic.ic_profile.save_ap_info[(short)uVar2].password,
						 g_ic.ic_profile.sta.password,sizeof(g_ic.ic_profile.sta.password)), iVar1 == 0)) {
				g_ic.ic_profile.save_ap_channel_info[uVar2] = channel;
				break;
			}
			uVar2 = uVar2 + 1 & 0xff;
		} while (uVar3 != uVar2);
	}
	if ((PmkCurrent == '\x01') &&
		 (param = os_malloc(sizeof(*param)),
		 param != NULL)) {
		system_param_load(system_param_sector_start,0,param,sizeof(*param));
		memcpy(&param->sta,&g_ic.ic_profile.sta,sizeof(param->sta));
		if (uVar2 < uVar3) {
			param->save_ap_channel_info[uVar2] = g_ic.ic_profile.save_ap_channel_info[uVar2];
		}
		system_param_save_with_protect(system_param_sector_start,param,sizeof(*param));
		os_free(param);
	}
}

void ICACHE_FLASH_ATTR
system_station_got_ip_set(ip_addr_st *ip, ip_addr_st *mask, ip_addr_st *gw)
{
	uint32_t uVar1;
	bool bVar2;
	uint32_t *param;
	int iVar3;
	uint32_t uVar4;

	struct netif *netif = g_ic.ic_if0_conn->ni_ifp;
	if (((netif != NULL) && (event_cb != NULL)) &&
			((ip->addr != (netif->ip_addr).addr ||
			 ((mask->addr != (netif->netmask).addr ||
			  (gw->addr != (netif->gw).addr)))))) {
		System_Event_st *sev = os_zalloc(sizeof(*sev));
		if (sev != NULL) {
			sev->event = EVENT_GOT_IP;
			sev->event_info.got_ip.ip = netif->ip_addr;
			sev->event_info.got_ip.mask = netif->netmask;
			sev->event_info.got_ip.gw = netif->gw;
			if (ets_post(PRIO_EVENT,EVENT_GOT_IP,(ETSParam)sev) != 0) {
				os_free(sev);
			}
		}
	}
	os_printf_plus("ip:" IPSTR ",mask:" IPSTR ",gw:" IPSTR "\n",
		IP2STR(&netif->ip_addr),
		IP2STR(&netif->netmask),
		IP2STR(&netif->gw));
	(g_ic.ic_if0_conn)->ni_connect_status = 5;
	(g_ic.ic_if0_conn)->ni_connect_step = 5;
	if ((g_ic.ic_profile.led.open_flag == 1) && (g_ic.ic_profile.opmode == 1)) {
		ets_timer_disarm(&sta_con_timer);
		int pin = g_ic.ic_profile.led.gpio_id & 0x1f;
		int mask = 1 << pin;
		if (status_led_output_level) {
			gpio_output_set(mask, 0, mask, 0);
		} else {
			gpio_output_set(0, mask, mask, 0);
		}
	}
}

bool ICACHE_FLASH_ATTR
wifi_station_get_auto_connect(void)
{
	if (g_ic.ic_profile.auto_connect < 2) {
		return g_ic.ic_profile.auto_connect;
	}
	return true;
}

void ICACHE_FLASH_ATTR
wifi_station_save_pmk2cache(void)
{
	wl_profile_st *param;
	int ap_num = g_ic.ic_profile.ap_change_param.ap_number;
	int i;
	if (ap_num == 0) {
		i = 0;
	}
	else {
		i = 0;
		do {
			if ((memcmp(g_ic.ic_profile.save_ap_info[i].ssid.ssid,
				    g_ic.ic_profile.sta.ssid.ssid,0x20) == 0) && 
				 (memcmp(g_ic.ic_profile.save_ap_info[i].password,
					 g_ic.ic_profile.sta.password,0x40) == 0)) {
				memcpy(g_ic.ic_profile.pmk_info + i,g_ic.ic_profile.sta.pmk,0x20);
				break;
			}
			i++;
		} while (i < ap_num);
	}
	if ((PmkCurrent == 1) && (param = os_malloc(sizeof(*param)), param != NULL)) {
		system_param_load(system_param_sector_start,0,param,sizeof(*param));
		memcpy(&param->sta,&g_ic.ic_profile.sta,0xa4);
		if (i < ap_num) {
			memcpy(param->pmk_info + i,g_ic.ic_profile.pmk_info + i,0x20);
		}
		system_param_save_with_protect(system_param_sector_start,param,sizeof(*param));
		os_free(param);
	}
}

void ICACHE_FLASH_ATTR
wifi_station_set_default_hostname(char *macaddr)
{
	if (hostname == NULL) {
		hostname = os_malloc(0x20);
	}
	if (hostname == NULL) {
		return;
	}
	ets_sprintf(hostname,"ESP-%02X%02X%02X",macaddr[3],macaddr[4],macaddr[5]);
}


uint8_t ieee80211_regdomain_max_chan(void);

bool ICACHE_FLASH_ATTR
wifi_get_macaddr(uint8 if_index,uint8 *macaddr)
{
	if ((if_index > 1) || (macaddr == NULL)) {
		return false;
	}

	struct netif *ni = eagle_lwip_getif(if_index);
	if (ni == NULL) {
		if (if_index == 0) {
			memcpy(macaddr,info.sta_mac,6);
		}
		else {
			memcpy(macaddr,info.softap_mac,6);
		}
	}
	else {
		memcpy(macaddr,ni->hwaddr,6);
	}
	return true;
}

bool ICACHE_FLASH_ATTR
wifi_softap_get_config_local(softap_config_st *config,uint8_t current)
{
	uint8_t bVar1;
	uint16_t uVar2;
	auth_mode_t authmode;
	int iVar3;
	uint8_t bVar4;
	bool res;
	wl_profile_st *param;
	char ssid_fmt[32];
	uint8_t mac[5];
	uint32_t _current;
	uint8_t *password;
	
	_current = current;
	if (config == NULL) {
		return false;
	}

	if (_current == 1) {
		param = &g_ic.ic_profile;
	}
	else {
		param = os_malloc(sizeof(*param));
		if (param == NULL) {
			return false;
		}
		system_param_load((uint16)system_param_sector_start,0,param,sizeof(*param));
	}
	authmode = (param->softap).authmode;
	if ((authmode < AUTH_WPA2_ENTERPRISE) && (authmode != AUTH_WEP)) {
		config->authmode = authmode;
		password = (param->softap).password;
	}
	else {
		config->authmode = AUTH_OPEN;
		password = (param->softap).password;
		bzero(password,sizeof(param->softap.password));
	}
	if (((param->softap).ssid.len == -1) || ((param->softap).ssid.ssid[0] == 0xff)) {
		wifi_get_macaddr('\x01',mac);
		bzero(&param->softap.ssid,sizeof(param->softap.ssid));
		ets_sprintf((param->softap).ssid.ssid,"ESP-%02X%02X%02X",mac[4],mac[5],mac[6]);
		(param->softap).ssid.len = 10;
	}
	memcpy(config->ssid,(param->softap).ssid.ssid,sizeof(config->ssid));
	memcpy(config->password,password,sizeof(config->password));
	iVar3 = (param->softap).ssid.len;
	if ((iVar3 < sizeof(param->softap.ssid.ssid)) && (0 < iVar3)) {
		config->ssid_len = (uint8)iVar3;
	}
	else {
		config->ssid_len = '\0';
	}
	bVar4 = ieee80211_regdomain_max_chan();
	bVar1 = (param->softap).channel;
	if ((bVar4 < bVar1) || (bVar1 == 0)) {
		bVar1 = 1;
	}
	config->channel = bVar1;
	bVar1 = (param->softap).ssid_hidden;
	bVar4 = 0;
	if (bVar1 < 2) {
		bVar4 = bVar1;
	}
	config->ssid_hidden = bVar4;
	bVar1 = (param->softap).max_connection;
	bVar4 = 8;
	if (bVar1 < 9) {
		bVar4 = bVar1;
	}
	config->max_connection = bVar4;
	uVar2 = param->softap_beacon_interval;
	if ((60000 < uVar2) || (uVar2 < 100)) {
		uVar2 = 100;
	}
	config->beacon_interval = uVar2;
	if (_current != 1) {
		os_free(param);
	}
	return true;
}

bool ICACHE_FLASH_ATTR
wifi_softap_get_config(softap_config_st *config)
{
	return wifi_softap_get_config_local(config,'\x01');
}

bool ICACHE_FLASH_ATTR
wifi_softap_set_default_ssid(void)
{
	uint8_t mac[6];

	wifi_get_macaddr('\x01',mac);
	if ((g_ic.ic_profile.softap.ssid.len == -1) || (g_ic.ic_profile.softap.ssid.ssid[0] == 0xff)) {
		bzero(&g_ic.ic_profile.softap,0x24);
		ets_sprintf(g_ic.ic_profile.softap.ssid.ssid,"ESP-%02X%02X%02X",mac[3],mac[4],mac[5]);
		g_ic.ic_profile.softap.ssid.len = 10;
	}
	return true;
}

int hexstr2bin(char *hex,uint8_t *buf,int len);
bool ieee80211_regdomain_chan_in_range(uint8_t chan);
uint8_t ieee80211_regdomain_min_chan(void);
void wDev_Set_Beacon_Int(uint32_t beacon_int);
void system_soft_wdt_stop(void);
void system_soft_wdt_restart(void);
int pbkdf2_sha1(const char *passphrase, const char *ssid, size_t ssid_len, int iterations, uint8_t *buf, size_t buflen);
void wifi_softap_start(uint8_t bcn_only);
void wifi_softap_stop(uint8_t bcn_only);

bool ICACHE_FLASH_ATTR
wifi_softap_set_config_local2(wl_profile_st *profile,softap_config_st *config,uint8_t current)
{
	bool bVar1;
	auth_mode_t new_authmode;
	bool bVar2;
	uint8_t chan;
	int ssid_len;
	int iVar3;
	size_t sVar4;
	uint8_t *ap_password;
	uint16_t uVar5;
	auth_mode_t authmode;
	uint8_t *ssid;
	uint8_t bVar6;
	uint8_t *password;
	
	ssid = (profile->softap).ssid.ssid;
	password = config->password;
	if (memcmp(ssid,config,0x20) == 0) {
		new_authmode = config->authmode;
		if (memcmp((profile->softap).password,password,0x40) == 0) {
			authmode = (profile->softap).authmode;
			if (new_authmode == authmode) {
				if (((config->channel == (profile->softap).channel) &&
						(config->max_connection == (profile->softap).max_connection)) &&
					 ((config->ssid_hidden == (profile->softap).ssid_hidden &&
						(config->beacon_interval == profile->softap_beacon_interval)))) {
					return true;
				}
				bVar1 = false;
			}
			else if ((authmode == AUTH_OPEN) &&
							(((new_authmode == AUTH_WPA_PSK || (new_authmode == AUTH_WPA2_PSK)) ||
							 (new_authmode == AUTH_WPA_WPA2_PSK)))) {
				bVar1 = true;
			}
			else {
				bVar1 = false;
			}
			goto LAB_4023e640;
		}
	}
	else {
		new_authmode = config->authmode;
	}
	bVar1 = true;
LAB_4023e640:
	if ((AUTH_WEP < new_authmode) &&
		 ((sVar4 = strlen((char *)password), (int)sVar4 < 8 ||
			((sVar4 = strlen((char *)password), 0x3f < (int)sVar4 &&
			 (iVar3 = hexstr2bin((char *)password,(profile->softap).pmk,0x20), iVar3 == -1)))))) {
		return false;
	}
	if ((config->ssid[0] == 0xff) && (config->ssid[1] == 0xff)) {
		wifi_softap_set_default_ssid();
	}
	else {
		ssid_len = config->ssid_len;
		if (ssid_len == 0) {
			sVar4 = strlen((char *)config);
			ssid_len = sVar4 & 0xff;
		}
		if (0x20 < ssid_len) {
			ssid_len = 0x20;
		}
		(profile->softap).ssid.len = ssid_len;
		memcpy(ssid,config,0x20);
	}
	ap_password = (profile->softap).password;
	memcpy(ap_password,password,0x40);
	(profile->softap).password[0x40] = '\0';
	new_authmode = config->authmode;
	if ((new_authmode < AUTH_MAX) && (new_authmode != AUTH_WEP)) {
		(profile->softap).authmode = new_authmode;
	}
	else {
		(profile->softap).authmode = '\0';
	}
	bVar2 = ieee80211_regdomain_chan_in_range(config->channel);
	if (bVar2) {
		chan = config->channel;
	}
	else {
		chan = ieee80211_regdomain_min_chan();
	}
	(profile->softap).channel = chan;
	bVar6 = 0;
	if (config->ssid_hidden < 2) {
		bVar6 = config->ssid_hidden;
	}
	(profile->softap).ssid_hidden = bVar6;
	bVar6 = 8;
	if (config->max_connection < 9) {
		bVar6 = config->max_connection;
	}
	(profile->softap).max_connection = bVar6;
	int beacon_int = config->beacon_interval;
	if ((60000 < beacon_int) || (beacon_int < 100)) {
		beacon_int = 100;
	}
	profile->softap_beacon_interval = beacon_int;
	if (current == '\0') {
		int x = beacon_int / 100;
		wDev_Set_Beacon_Int((x & 0xffff) * 0x19000);
	}
	if (((AUTH_WEP < config->authmode) && (sVar4 = strlen((char *)ap_password), (int)sVar4 < 0x40)) &&
		 (bVar1)) {
		system_overclock();
		system_soft_wdt_stop();
		pbkdf2_sha1(ap_password,ssid,(profile->softap).ssid.len,0x1000,(profile->softap).pmk,0x20);
		system_soft_wdt_restart();
		system_restoreclock();
	}
	if (current == '\x01') {
		system_param_save_with_protect(system_param_sector_start,profile,0x4a4);
	}
	if ((current == '\0') && (g_ic.ic_if1_conn != NULL)) {
		if (g_ic.ic_mode == '\0') {
			wifi_softap_stop('\0');
			wifi_softap_start(0);
		}
		else {
			BcnEb_update = '\x01';
		}
	}
	return true;
}

bool ICACHE_FLASH_ATTR
wifi_softap_set_config_local(softap_config_st *config, uint8_t current)
{
	bool bVar2;
	int iVar3;
	wl_profile_st *profile;
	
	int opmode = wifi_get_opmode();
	if ((((config == NULL) || (opmode == '\x01')) || (opmode == '\0')) || (g_ic.ic_mode == '\x01')) {
		return false;
	}

	iVar3 = CheckSleep('\x06');
	if (iVar3 == -1) {
		memcpy(&SleepDeferSoftAPConfig,config,0x6c);
		SleepDeferCurrent6 = current;
		return true;
	}
	if (current == '\x01') {
		profile = os_malloc(sizeof(*profile));
		if (profile == NULL) {
			return false;
		}
		system_param_load(system_param_sector_start,0,profile,sizeof(*profile));
		wifi_softap_set_config_local2(profile,config,'\x01');
		os_free(profile);
	}
	return wifi_softap_set_config_local2(&g_ic.ic_profile,config,'\0');
}


int ICACHE_FLASH_ATTR
wifi_softap_set_station_info(char *mac, ip_addr_t *addr)
{
	ieee80211_conn_st *conn;
	System_Event_st *param;

	conn = g_ic.ic_if1_conn;
	int max_conn = g_ic.ic_profile.softap.max_connection;
	int bss_idx = 1; /* off-by-one? */
	do {
		if (conn->ni_ap_bss[bss_idx] != NULL) {
			if (memcmp(mac,conn->ni_ap_bss[bss_idx]->ni_bssid,6) == 0) {
				conn->ni_ap_bss[bss_idx]->ni_ip = addr->addr;
				if ((event_cb != NULL) && (param = os_zalloc(sizeof(*param)), param != NULL)) {
					param->event = EVENT_DISTRIBUTE_STA_IP;
					param->event_info.distribute_sta_ip.aid = conn->ni_ap_bss[bss_idx]->ni_associd;
					memcpy(&param->event_info.distribute_sta_ip.mac,mac,6);
					param->event_info.distribute_sta_ip.ip = *addr;
					if (ets_post(PRIO_EVENT,EVENT_DISTRIBUTE_STA_IP,(ETSParam)param) != 0) {
						os_free(param);
					}
				}
				return 1;
			}
		}
		bss_idx = bss_idx + 1 & 0xff;
	} while (bss_idx < max_conn + 2);
	return 0;
}


typedef enum dhcp_status {
	DHCP_STOPPED=0,
	DHCP_STARTED=1
} dhcp_status_t;

dhcp_status_t ICACHE_FLASH_ATTR
wifi_softap_dhcps_status(void)
{
	return dhcps_flag;
}

dhcp_status_t ICACHE_FLASH_ATTR
wifi_station_dhcpc_status(void)
{
	return dhcpc_flag;
}

bool wifi_station_get_reconnect_policy(void);
void dhcp_start(struct netif *netif);
void dhcp_stop(struct netif *netif);
void dhcp_cleanup(struct netif *netif);

void ICACHE_FLASH_ATTR
wifi_station_dhcpc_event(void)
{
	if (event_cb != NULL) {
		struct netif *netif = eagle_lwip_getif('\0');
		if (netif != NULL) {
			dhcp_stop(netif);
			dhcp_cleanup(netif);
		}
		System_Event_st *sev = os_zalloc(sizeof(*sev));
		if (sev != NULL) {
			sev->event = 4;
			if (ets_post(PRIO_EVENT,4,(ETSParam)sev) != 0) {
				os_free(sev);
			}
		}
	}
	if (wifi_station_get_reconnect_policy()) {
		wifi_station_disconnect();
		wifi_station_connect();
	}
}

int ieee80211_send_mgmt(ieee80211_conn_st *conn, int type, int arg);
void cnx_node_leave(ieee80211_conn_st *ni, ieee80211_bss_st *bss);

bool ICACHE_FLASH_ATTR
wifi_softap_deauth(uint8_t *da)
{
	ieee80211_conn_st *conn;
	
	conn = g_ic.ic_if1_conn;
	uint8_t opmode = wifi_get_opmode();
	if ((((opmode == '\x01') || (opmode == '\0')) || (conn == NULL)) ||
		 (g_ic.ic_mode != '\0')) {
		return false;
	}

	int max_conn = g_ic.ic_profile.softap.max_connection;;
	int i;
	ieee80211_bss_st *conn_bss = conn->ni_bss;
	conn->ni_bss = conn->ni_ap_bss[0];
	if (da == NULL) {
		/* deauth all */
		i = 1;
		do {
			ieee80211_bss_st *__src = conn->ni_ap_bss[i];
			if ((__src != NULL) && ((__src->ni_flags >> 4 & 1) == 0)) {
				memcpy(conn->ni_macaddr,__src,6);
				ieee80211_send_mgmt(conn,0xc0,2);
			}
			i++;
		} while (i < max_conn + 2);
	}
	else {
		/* deauth single */
		memcpy(conn->ni_macaddr,da,6);
		ieee80211_send_mgmt(conn,0xc0,2);
	}
	conn->ni_bss = conn_bss;
	i = 1;
	do {
		conn_bss = conn->ni_ap_bss[i];
		if (conn_bss != NULL) {
			if (da == NULL) {
LAB_4023ec11:
				cnx_node_leave(conn,conn_bss);
			}
			else {
				if (memcmp(da,conn_bss,6) == 0) {
					conn_bss = conn->ni_ap_bss[i];
					goto LAB_4023ec11;
				}
			}
		}
		i++;
	} while (i < max_conn + 2);
	return true;
}

void ICACHE_FLASH_ATTR
wifi_softap_cacl_mac(uint8_t *dst, const uint8_t *src)
{
	if (dst == NULL || src == NULL) {
		return;
	}

	memcpy(dst,src,6);
	for (int xor = 0; xor < 0x100; xor += 4) {
		*dst = (*src | 2) ^ xor;
		if (*src != *dst) {
			return;
		}
	}
}

bool open_signaling_measurement;

void ICACHE_FLASH_ATTR
wifi_enable_signaling_measurement(void)
{
	open_signaling_measurement = true;
}

void ICACHE_FLASH_ATTR
wifi_disable_signaling_measurement(void)
{
	open_signaling_measurement = false;
}

enum phy_mode {
    PHY_MODE_11B    = 1,
    PHY_MODE_11G    = 2,
    PHY_MODE_11N    = 3
};

enum phy_mode ICACHE_FLASH_ATTR
wifi_get_phy_mode(void)
{
	return g_ic.ic_profile.phyMode;
}

union aligned_data {
	uint32_t x;
	uint8_t u8[4];
	uint16_t u16[2];
};

int ICACHE_FLASH_ATTR
system_get_data_of_array_8(const unsigned char *array, int ofs)
{
	uint32_t addr = (uint32_t)(array + ofs);
	uint32_t *aligned = (uint32_t*)(addr & ~3);
	int shift = 8 * (addr & 3);
	return (*aligned >> shift) & 0xff;
}

bool ICACHE_FLASH_ATTR
wifi_get_ip_info(uint8_t if_index, struct ip_info *ip_info)
{
	if ((if_index > 1) || (ip_info == NULL)) {
		return false;
	}

	struct netif *netif = eagle_lwip_getif(if_index);
	if ((netif == NULL) || ((netif->flags & 1) == 0)) {
		ip_info->ip.addr = 0;
		ip_info->netmask.addr = 0;
		ip_info->gw.addr = 0;
		if (if_index == 0) {
			if (!dhcpc_flag) {
				*ip_info = info.sta_info;
			}
		}
		else if (!dhcps_flag) {
			*ip_info = info.softap_info;
		}
	}
	else {
		(ip_info->ip).addr = (netif->ip_addr).addr;
		(ip_info->netmask).addr = (netif->netmask).addr;
		(ip_info->gw).addr = (netif->gw).addr;
	}
	return true;
}

uint8_t ICACHE_FLASH_ATTR
wifi_get_opmode_local(bool current)
{
	uint8_t mode;
	wl_profile_st *param;
	
	if (current) {
		param = &g_ic.ic_profile;
	}
	else {
		param = os_malloc(sizeof(*param));
		if (param == NULL) {
			return 0;
		}
		system_param_load(system_param_sector_start,0,param,sizeof(*param));
	}
	mode = param->opmode;
	if (3 < mode) {
		mode = 2;
	}
	if (!current) {
		os_free(param);
	}
	return mode;
}

void wifi_mode_set(uint8_t opmode);
void wifi_station_start(void);
void wifi_station_stop(void);
void netif_set_default(struct netif *ni);

void ICACHE_FLASH_ATTR
wifi_set_opmode_local_2(uint8_t opmode)
{
	if (opmode > 3) {
		opmode = 2;
	}
	if ((opmode == 1) || (opmode == 0)) {
		wifi_softap_stop('\0');
	}
	if ((opmode == 2) || (opmode == 0)) {
		wifi_station_stop();
	}
	wifi_mode_set(opmode);
	if ((opmode == 1) || (opmode == 3)) {
		wifi_station_start();
	}
	if ((opmode == 2) || (opmode == 3)) {
		wifi_softap_start(0);
	}
	if (opmode == 1) {
		netif_set_default((g_ic.ic_if0_conn)->ni_ifp);
	}
}

bool get_fpm_auto_sleep_flag(void);
void fpm_set_type_from_upper(uint8_t typ);
void fpm_open(void);
int8_t wifi_fpm_do_sleep(uint32_t sleep_time_in_us);

bool ICACHE_FLASH_ATTR
wifi_set_opmode_local(uint8_t new_opmode, uint8_t current_opmode)
{
	uint8_t auto_sleep_flag;
	bool res;
	int sleepval;
	wl_profile_st *wl_profile;
	System_Event_st *sev;
	uint8_t _current_opmode;
	uint8_t _new_opmode;
	
	if ((new_opmode < 4) && (g_ic.ic_mode == '\0')) {
		sleepval = CheckSleep(SLEEP_DEFER_SET_OPMODE);
		_new_opmode = new_opmode;
		_current_opmode = current_opmode;
		if (sleepval != -1) {
			wl_profile = os_malloc(sizeof(*wl_profile));
			if (wl_profile == NULL) goto LAB_4023d0ad;
			system_param_load(system_param_sector_start,0,wl_profile,sizeof(*wl_profile));
			_new_opmode = g_ic.ic_profile.opmode;
			if (new_opmode != g_ic.ic_profile.opmode) {
				OpmodChgIsOnGoing = 1;
				if (user_init_flag == '\x01') {
					g_ic.ic_profile.opmode = new_opmode;
					wifi_set_opmode_local_2(new_opmode);
				}
				OpmodChgIsOnGoing = 0;
				g_ic.ic_profile.opmode = new_opmode;
			}
			if ((current_opmode == '\x01') && (wl_profile->opmode != new_opmode)) {
				wl_profile->opmode = new_opmode;
				system_param_save_with_protect((uint16)system_param_sector_start,wl_profile,sizeof(*wl_profile));
				g_ic.ic_profile.opmode = new_opmode;
			}
			os_free(wl_profile);
			if ((event_cb != NULL) && (sev = os_zalloc(sizeof(*sev)), sev != NULL)) {
				sev->event = EVENT_OPMODE_CHANGED;
				(sev->event_info).opmode_changed.old_opmode = _new_opmode;
				(sev->event_info).opmode_changed.new_opmode = g_ic.ic_profile.opmode;
				if (ets_post(PRIO_EVENT,EVENT_OPMODE_CHANGED,(ETSParam)sev) != 0) {
					os_free(sev);
				}
			}
			_new_opmode = SleepDeferOpmode;
			_current_opmode = SleepDeferCurrent5;
			if ((new_opmode == '\0') &&
				 (auto_sleep_flag = get_fpm_auto_sleep_flag(), _new_opmode = SleepDeferOpmode,
				 _current_opmode = SleepDeferCurrent5, auto_sleep_flag == '\x01')) {
				fpm_set_type_from_upper('\x02');
				fpm_open();
				wifi_fpm_do_sleep(0xfffffff);
				_new_opmode = SleepDeferOpmode;
				_current_opmode = SleepDeferCurrent5;
			}
		}
		SleepDeferCurrent5 = _current_opmode;
		SleepDeferOpmode = _new_opmode;
		res = true;
	}
	else {
LAB_4023d0ad:
		res = false;
	}
	return res;
}

uint8_t ICACHE_FLASH_ATTR
wifi_get_opmode(void)
{
	return wifi_get_opmode_local(true);
}

bool ICACHE_FLASH_ATTR
wifi_set_opmode(uint8_t opmode)
{
	return wifi_set_opmode_local(opmode, true);
}

uint8_t ICACHE_FLASH_ATTR
wifi_station_get_connect_status(void)
{
	ieee80211_conn_st *conn = g_ic.ic_if0_conn;
	uint8_t opmode = wifi_get_opmode();

	if (((opmode == 2) || (opmode == 0)) || (conn == NULL)) {
		return -1;
	}
	return conn->ni_connect_status;
}

static bool reconnect_internal;

bool ICACHE_FLASH_ATTR
wifi_station_get_reconnect_policy(void)
{
	return reconnect_internal;
}

bool ICACHE_FLASH_ATTR
wifi_station_set_reconnect_policy(bool set)
{
	reconnect_internal = set;
	return true;
}

void cnx_sta_connect_cmd(wl_profile_st *prof, uint8_t desChan);

bool ICACHE_FLASH_ATTR
wifi_station_connect(void)
{
	ieee80211_conn_st *if0_conn = g_ic.ic_if0_conn;
	uint8_t opmode = wifi_get_opmode();
	if ((((opmode == '\x02') || (opmode == '\0')) || (if0_conn == NULL)) ||
		 (g_ic.ic_mode != '\0')) {
		return false;
	}

	if (CheckSleep(SLEEP_DEFER_CONNECT) != -1) {
		(g_ic.ic_if0_conn)->ni_connect_step = '\0';
		int ssid_len = g_ic.ic_profile.sta.ssid.len;
		(g_ic.ic_if0_conn)->ni_connect_err_time = '\0';
		if ((ssid_len != -1) && (ssid_len != 0)) {
			uint32_t chancfg = RTCMEM->SYSTEM_CHANCFG;
			if (chancfg & 0x10000) {
				if ((chancfg & 0xff) < 14) {
					cnx_sta_connect_cmd(&g_ic.ic_profile,chancfg & 0xff);
				}
				else {
					cnx_sta_connect_cmd(&g_ic.ic_profile,'\0');
				}
			}
			else {
				cnx_sta_connect_cmd(&g_ic.ic_profile,'\0');
			}
		}
	}
	return true;
}

void scan_cancel(void);
void ic_set_vif(uint8_t index, uint8_t set, uint8_t *mac, ieee80211_opmode_t op_mode, bool is_p2p);
int ieee80211_sta_new_state(ieee80211com_st *ic, ieee80211_state_t nstate, int arg);

bool ICACHE_FLASH_ATTR
wifi_station_disconnect(void)
{
	ieee80211_conn_st *conn = g_ic.ic_if0_conn;
	uint8_t opmode = wifi_get_opmode();
	if (((conn == NULL) || (g_ic.ic_mode != '\0')) ||
		 (((opmode == '\x02' || (opmode == '\0')) && (OpmodChgIsOnGoing == '\0')))) {
		return false;
	}

	if (CheckSleep(SLEEP_DEFER_DISCONNECT) != -1) {
		(g_ic.ic_if0_conn)->ni_connect_step = '\0';
		(g_ic.ic_if0_conn)->ni_connect_status = '\0';
		(g_ic.ic_if0_conn)->ni_connect_err_time = '\0';
		if (deep_sleep_flag == false) {
			RTCMEM->SYSTEM_CHANCFG = 0x10000;
		}
		scan_cancel();
		if ((g_ic.ic_if0_conn)->ni_mlme_state == IEEE80211_S_INIT) {
			ets_timer_disarm(&(g_ic.ic_if0_conn)->ni_connect_timeout);
			ets_timer_disarm(&(g_ic.ic_if0_conn)->ni_beacon_timeout);
		}
		else {
			ieee80211_sta_new_state(&g_ic,IEEE80211_S_INIT,0);
		}
		if (((g_ic.ic_if0_conn)->ni_connect_step & (1 << 0x19)) == 0) {
			eagle_lwip_if_free(conn);
			ic_set_vif('\0','\0',NULL,IEEE80211_M_STA,false);
		}
	}

	return true;
}

void ICACHE_FLASH_ATTR
Event_task(ETSEvent *events)
{
	os_printf_plus("Event_task(%d, %p)\n", events->sig, events->par);
	System_Event_st *sev = (System_Event_st *)events->par;
	if ((event_cb != NULL) && (sev != NULL)) {
		event_cb(sev);
		os_free(sev);
	}
}

static ETSEvent event_TaskQueue[0x20];

void ICACHE_FLASH_ATTR
wifi_set_event_handler_cb(wifi_event_handler_cb_t cb)
{
	static bool event_flag = false;
	if (!event_flag) {
		ets_task(Event_task,PRIO_EVENT,event_TaskQueue,(uint8_t)ARRAY_SIZE(event_TaskQueue));
		event_flag = true;
	}
	event_cb = cb;
}

bool ICACHE_FLASH_ATTR
system_os_task(ETSTask task,uint8_t prio,ETSEvent *queue,uint8_t qlen)
{
	if (prio < 3) {
		if ((queue != NULL) && (qlen != 0)) {
			ets_task(task,prio + 2 /* bug? */, queue, qlen);
			return true;
		}
		os_printf_plus("err: task queue error\n");
	}
	else {
		os_printf_plus("err: task prio < %d\n", 3);
	}
	return false;
}

void user_uart_wait_tx_fifo_empty(uint8_t uart_no, uint32_t time_out_us);

void ICACHE_FLASH_ATTR
system_uart_swap(void)
{
	user_uart_wait_tx_fifo_empty(0,500000);
	user_uart_wait_tx_fifo_empty(1,500000);
	/* 0x60000808 / UART0_CTS */
	IOMUX->GPIO13 = IOMUX->GPIO13 & 0xfffffecf | 0x100;
	/* 0x60000810 / UART0_RTS */
	IOMUX->GPIO15 = IOMUX->GPIO13 & 0xfffffecf | 0x100;
	/* Swap UART0 pins (u0rxd <-> u0cts), (u0txd <-> u0rts) */
	DPORT->PERI_IO_SWAP |= 4;
}

void ICACHE_FLASH_ATTR
system_uart_de_swap(void)
{
	user_uart_wait_tx_fifo_empty(0,500000);
	user_uart_wait_tx_fifo_empty(1,500000);
	/* Unswap UART0 pins (u0rxd <-> u0cts), (u0txd <-> u0rts) */
	DPORT->PERI_IO_SWAP &= ~4;
}

extern char *SDK_VERSION;

char* ICACHE_FLASH_ATTR
system_get_sdk_version(void)
{
	return SDK_VERSION;
}

/*
         U BcnEb_update
         U _bss_end
         U _bss_start
         U Cache_Read_Disable
         U chm_get_current_channel
         U chm_set_current_channel
         U clockgate_watchdog
         U cnx_node_leave
         U cnx_sta_connect_cmd
         U cnx_sta_scan_cmd
         U _data_end
         U _data_start
         U deep_sleep_set_option
         U default_hostname
         U dhcp_cleanup
         U dhcps_start
         U dhcps_stop
         U dhcp_start
         U dhcp_stop
         U __divsi3
         U eagle_lwip_getif
         U eagle_lwip_if_free
         U ets_bzero
         U ets_delay_us
         U ets_get_cpu_frequency
         U ets_intr_lock
         U ets_intr_unlock
         U ets_isr_mask
         U ets_memcmp
         U ets_memcpy
         U ets_memset
         U ets_post
         U ets_sprintf
         U ets_strcpy
         U ets_strlen
         U ets_strncmp
         U ets_task
         U ets_timer_arm_new
         U ets_timer_disarm
         U ets_timer_setfn
         U ets_update_cpu_frequency
         U ets_vprintf
         U ets_write_char
         U flashchip
         U fpm_allow_tx
         U fpm_do_wakeup
         U get_fpm_auto_sleep_flag
         U g_ic
         U gpio_output_set
         U gpio_pin_wakeup_disable
         U gpio_pin_wakeup_enable
         U gScanStruct
         U _heap_start
         U hexstr2bin
         U hostname
         U ic_get_rssi
         U ic_set_vif
         U ieee80211_freedom_output
         U ieee80211_ht_attach
         U ieee80211_phy_init
         U ieee80211_regdomain_chan_in_range
         U ieee80211_regdomain_get_country
         U ieee80211_regdomain_max_chan
         U ieee80211_regdomain_min_chan
         U ieee80211_rfid_locp_recv_close
         U ieee80211_rfid_locp_recv_open
         U ieee80211_send_mgmt
         U ieee80211_sta_new_state
         U info
         U __muldi3
         U netif_default
         U netif_set_addr
         U netif_set_default
         U no_ap_found_index
         U pbkdf2_sha1
         U phy_adc_read_fast
         U phy_afterwake_set_rfoption
         U phy_get_check_flag
         U phy_get_vdd33
         U phy_set_most_tpw
         U phy_set_powerup_option
         U phy_vdd33_set_tpw
         U pm_clear_gpio_wakeup_pin
         U pm_get_listen_interval
         U pm_get_sleep_level
         U pm_get_sleep_type
         U pm_is_open
         U pm_is_waked
         U pm_open_rf
         U pm_post
         U pm_rtc_clock_cali_proc
         U pm_set_gpio_wakeup_pin
         U pm_set_listen_interval
         U pm_set_sleep_cycles
         U pm_set_sleep_level
         U pm_set_sleep_type_from_upper
         U pm_usec2rtc_64
         U ppRecycleRxPkt
         U pvPortMalloc
         U pvPortZalloc
         U rc_set_rate_limit_id
         U register_ieee80211_rfid_locp_recv_cb
         U _rodata_end
         U _rodata_start
         U scan_cancel
         U scannum
         U SDK_VERSION
         U spi_flash_erase_sector
         U spi_flash_read
         U spi_flash_write
         U sta_con_timer
         U system_soft_wdt_restart
         U system_soft_wdt_stop
         U test_tout
         U __udivsi3
         U unregister_ieee80211_rfid_locp_recv_cb
         U user_init_flag
         U user_uart_wait_tx_fifo_empty
         U vPortFree
         U Wait_SPI_Idle
         U wDevDisableRx
         U wDevEnableRx
         U wdev_exit_sniffer
         U wdev_go_sniffer
         U wDev_Set_Beacon_Int
         U wDev_SetMacAddress
         U WdevTimOffSet
         U wifi_fpm_do_sleep
         U wifi_fpm_open
         U wifi_fpm_set_sleep_type
         U wifi_mode_set
         U wifi_softap_start
         U wifi_softap_stop
         U wifi_station_start
         U wifi_station_stop
         U xPortGetFreeHeapSize
         U xPortWantedSizeAlign
00000000 t mem_debug_file_76
00000000 d printf_level_71
00000000 b SleepFuncRegisterIni_85
00000004 b SleepDeferTimer_87
00000004 T system_get_free_heap_size
00000004 T system_get_os_print
00000004 D system_param_sector_start
00000004 T system_rtc_mem_read
00000004 T system_rtc_mem_write
00000004 T system_set_os_print
00000008 D bin_sum_len
00000008 T system_get_time
0000000c T system_os_post
0000000c D timer2_ms_flag
0000000d D dhcps_flag
0000000e D dhcpc_flag
0000000f d PmkCurrent_277
00000010 D reconnect_internal
00000014 T system_restart_core
00000018 b SleepDeferTimerRun_91
00000019 b DeferFuncNum_95
0000001a b CurDeferFuncIndex_96
0000001c b SleepDeferStationConfig_101
00000020 T default_ssid
00000038 T os_printf_plus
00000040 t flash_str$7075_54_3
00000040 T system_deep_sleep_local_2
00000054 t CheckSleep
00000070 t flash_str$7328_99_2
00000090 t flash_str$7424_104_10
00000090 b SleepDeferCurrent3_102
00000091 b SleepDeferOpmode_105
00000092 b SleepDeferCurrent5_106
00000094 b SleepDeferSoftAPConfig_108
000000b0 t flash_str$7427_104_13
000000d0 t flash_str$7428_104_14
000000f0 t flash_str$7430_104_16
00000100 b SleepDeferCurrent6_109
00000101 b upgrade_flag_153
00000102 B protect_flag
00000103 b deep_sleep_option_169
00000108 b deep_sleep_cycle_172
00000110 B deep_sleep_flag
00000110 t flash_str$7431_104_17
00000111 B cpu_overclock
00000114 B event_cb
00000118 B status_led_output_level
0000011c B done_cb
00000120 B rst_if
00000130 t flash_str$7566_112_9
0000013c B default_interface
0000013d B OpmodChgIsOnGoing
0000013e B open_signaling_measurement
00000140 B promiscuous_cb
00000144 t SleepDeferTimeOut
00000150 b DeferFuncIndexArr_97
00000150 t flash_str$7567_112_10
00000160 B event_TaskQueue
00000180 t flash_str$7631_110_10
000001a0 t flash_str$7632_110_11
000001c0 t flash_str$7668_113_6
000001d0 t flash_str$7704_114_22
000001e0 t flash_str$7711_114_23
00000200 t flash_str$7718_114_24
00000220 t flash_str$7894_115_7
00000230 t flash_str$7895_115_8
00000234 t flash_str$7896_115_9
00000238 t flash_str$7897_115_10
00000238 T system_pp_recycle_rx_pkt
0000023c t flash_str$7898_115_11
00000240 t flash_str$7899_115_12
00000244 t flash_str$7900_115_13
00000250 t flash_str$7961_119_1
00000258 T system_adc_read
00000260 b ap_id$9338_207_4
00000261 b _event_flag$11178_333_2
00000270 t flash_str$8040_123_3
00000290 t flash_str$8054_126_4
000002a0 T system_adc_read_fast
000002b0 t flash_str$8199_147_9
000002e0 t flash_str$8200_147_10
000002f0 t flash_str$8321_150_1
0000030c T system_get_vdd33
00000310 t flash_str$8322_150_2
00000330 t flash_str$8323_150_3
00000338 W system_restart_hook
00000350 t flash_str$8324_150_4
00000370 t flash_str$8745_186_2
00000384 T system_restart_local
00000390 t flash_str$8886_194_9
00000394 t flash_str$8888_194_10
00000398 t flash_str$8889_194_11
000003a0 t flash_str$8891_194_12
000003b0 t flash_str$9498_213_6
000003d0 t flash_str$10753_296_3
000003e0 t flash_str$11190_335_5
00000400 t flash_str$11191_335_7
00000420 t flash_str$11210_336_5
0000048c T system_restart
0000050c T system_restore
0000057c T system_get_flash_size_map
000005a0 T system_get_boot_version
000005b4 t system_check_boot
000005e0 T system_get_test_result
00000608 T system_get_userbin_addr
000006a8 T system_get_boot_mode
0000072c T system_restart_enhance
000008b0 T system_upgrade_userbin_set
00000900 T system_upgrade_userbin_check
00000940 T system_upgrade_flag_set
00000958 T system_upgrade_flag_check
00000968 T spi_flash_erase_protect_enable
00000994 T spi_flash_erase_protect_disable
000009bc T spi_flash_erase_sector_check
00000a70 T system_get_current_sumlength
00000af0 T get_irom0_bin_len
00000b80 T get_flash_bin_len
00000e90 T system_upgrade_reboot
00001014 t system_deep_sleep_instant_1
000011e8 T system_deep_sleep_instant
00001274 T system_deep_sleep
0000131c T system_deep_sleep_set_option
0000133c T system_phy_temperature_alert
00001354 T system_phy_set_max_tpw
0000136c T system_phy_set_tpw_via_vdd33
00001384 T system_phy_set_rfoption
0000139c T system_phy_set_powerup_option
000013bc T system_update_cpu_freq
00001418 T system_get_cpu_freq
00001434 T system_overclock
00001464 T system_restoreclock
0000149c T system_timer_reinit
000014bc T system_relative_time
00001518 T system_station_got_ip_set
00001664 T system_print_meminfo
000016d0 T system_get_chip_id
00001714 T system_rtc_clock_cali_proc
0000172c T system_get_rtc_time
00001754 T system_mktime
00001828 T system_init_done_cb
00001834 T system_get_rst_info
0000183c T system_get_data_of_array_8
00001860 T system_get_data_of_array_16
00001890 T system_get_string_from_flash
00001910 T wifi_softap_dhcps_start
00001970 T wifi_softap_dhcps_stop
000019b8 T wifi_softap_dhcps_status
000019d4 T wifi_station_dhcpc_start
00001a40 T wifi_station_dhcpc_stop
00001aac T wifi_station_dhcpc_event
00001b2c T wifi_station_dhcpc_set_maxtry
00001b40 T wifi_station_dhcpc_status
00001b60 t wifi_get_opmode_local
00001bcc T wifi_get_opmode
00001be0 T wifi_get_opmode_default
00001c10 t wifi_set_opmode_local_2
00001c84 T wifi_get_broadcast_if
00001cdc T wifi_set_broadcast_if
00001dc4 t wifi_set_opmode_local
00001ef4 T wifi_set_opmode
00001f08 T wifi_set_opmode_current
00001f1c T system_get_checksum
00001f94 T wifi_param_save_protect_with_check
0000208c T system_param_save_with_protect
000021a8 T system_save_sys_param
000021d4 T system_param_load
00002274 t wifi_station_get_config_local
0000234c T wifi_station_get_config
00002360 T wifi_station_get_config_default
00002394 T wifi_station_get_ap_info
00002470 T wifi_station_ap_number_set
0000252c T wifi_station_save_ap_channel
00002650 t wifi_station_set_config_local_2
00002834 t wifi_station_set_config_local_3
00002910 t wifi_station_set_config_local_1
00002aa4 t wifi_station_set_config_local
00002b50 T wifi_station_set_config
00002b6c T wifi_station_set_config_current
00002b84 T wifi_station_restore_config
00002b9c T wifi_station_get_current_ap_id
00002bb4 T wifi_station_ap_check
00002c40 T wifi_station_ap_change
00002d68 T wifi_station_scan
00002dbc T wifi_station_get_auto_connect
00002e00 T wifi_station_set_auto_connect
00002ee0 T wifi_station_save_pmk2cache
00003010 T wifi_station_save_bssid
000030b0 T wifi_station_connect
00003188 T wifi_station_disconnect
00003258 T wifi_station_get_connect_status
0000328c T wifi_station_set_reconnect_policy
000032a8 T wifi_station_get_reconnect_policy
000032b8 T wifi_station_get_rssi
00003328 T wifi_station_set_default_hostname
00003394 T wifi_station_get_hostname
000033f0 T wifi_station_set_hostname
00003494 T wifi_softap_cacl_mac
00003514 T wifi_softap_set_default_ssid
000035b4 t wifi_softap_get_config_local
00003724 T wifi_softap_get_config
00003738 T wifi_softap_get_config_default
000037b0 t wifi_softap_set_config_local2
00003a2c t wifi_softap_set_config_local
00003acc T wifi_softap_set_config
00003ae0 T wifi_softap_set_config_current
00003b28 T wifi_softap_set_station_info
00003c20 T wifi_softap_get_station_info
00003cdc T wifi_softap_free_station_info
00003d24 T wifi_softap_get_station_num
00003d94 T wifi_softap_deauth
00003e88 T wifi_softap_get_beacon_only_mode
00003eb4 T wifi_softap_set_beacon_only_mode
00003f24 T wifi_register_user_ie_manufacturer_recv_cb
00003f3c T wifi_unregister_user_ie_manufacturer_recv_cb
00003fa4 T wifi_set_user_ie
000040bc T wifi_get_user_ie
000040f0 T wifi_get_phy_mode
0000415c T wifi_set_phy_mode
00004270 T wifi_enable_signaling_measurement
00004280 T wifi_disable_signaling_measurement
00004290 T wifi_set_sleep_type
000042b4 T wifi_set_sleep_level
000042d0 T wifi_set_listen_interval
000042ec T wifi_enable_gpio_wakeup
0000431c T wifi_disable_gpio_wakeup
00004338 T wifi_get_sleep_type
00004350 T wifi_get_sleep_level
00004368 T wifi_get_listen_interval
00004380 T wifi_get_channel
000043b4 T wifi_set_channel
00004434 T wifi_adjust_ap_chan
000044b8 T wifi_set_country
00004504 T wifi_get_country
00004548 T wifi_promiscuous_set_mac
000045dc T wifi_promiscuous_enable
000046d0 T wifi_set_promiscuous_rx_cb
000046e8 T wifi_get_ip_info
0000477c T wifi_set_ip_info
00004808 T wifi_get_macaddr
000048bc T wifi_set_macaddr
000049f4 T wifi_enable_6m_rate
00004a00 T wifi_get_user_fixed_rate
00004a24 T wifi_set_user_fixed_rate
00004a48 T wifi_set_user_sup_rate
00004a88 T wifi_set_user_rate_limit
00004ab0 T wifi_get_user_limit_rate_mask
00004abc T wifi_set_user_limit_rate_mask
00004ad8 T wifi_register_send_pkt_freedom_cb
00004af0 T wifi_unregister_send_pkt_freedom_cb
00004b04 T wifi_send_pkt_freedom
00004b54 T wifi_rfid_locp_recv_open
00004b6c T wifi_rfid_locp_recv_close
00004b84 T wifi_register_rfid_locp_recv_cb
00004b9c T wifi_unregister_rfid_locp_recv_cb
00004bb4 T wifi_status_led_install
00004bf4 T wifi_status_led_uninstall
00004c1c T wifi_set_status_led_output_level
00004c40 t Event_task
00004c88 T wifi_set_event_handler_cb
00004cdc T system_os_task
00004d30 T system_uart_swap
00004da8 T system_uart_de_swap
00004de4 T system_get_sdk_version
*/

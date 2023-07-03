#include "ets_sys.h"
#include "driver/uart.h"
#include "osapi.h"
#include "ip_addr.h"
#include "os_type.h"
#include "gpio.h"
#include "eagle_soc.h"
#include "user_interface.h"
#include "user_config.h"
#include "mem.h"


static const partition_item_t at_partition_table[] = { // 16M 512+512 (4MiB)
	{ SYSTEM_PARTITION_BOOTLOADER, 		0x000000,	0x01000},
	{ SYSTEM_PARTITION_OTA_1, 			0x010000, 	0x10000},
	{ SYSTEM_PARTITION_OTA_2,			0x020000, 	0x10000},
	// Non-standard location for rfcal (was 0x3fb000, now 0x00d000)
	{ SYSTEM_PARTITION_RF_CAL,  		0x00d000, 	0x01000},
	// Non-standard location for phydata (was 0x3fc000, now 0x00f000)
	{ SYSTEM_PARTITION_PHY_DATA, 		0x00f000, 	0x01000},
	{ SYSTEM_PARTITION_SYSTEM_PARAMETER, 	0x3fd000, 	0x03000},
};

static const char* ICACHE_FLASH_ATTR
auth_mode_str(AUTH_MODE mode)
{
	switch (mode) {
	case AUTH_OPEN:         return "OPEN";
	case AUTH_WEP:          return "WEP";
	case AUTH_WPA_PSK:      return "WPA_PSK";
	case AUTH_WPA2_PSK:     return "WPA2_PSK";
	case AUTH_WPA_WPA2_PSK: return "WPA_WPA2_PSK";
	}
	return "UNKNOWN";
}

static void ICACHE_FLASH_ATTR
print_ap_config(void)
{
	struct softap_config config = {0};
	struct ip_info ip = {0};

	wifi_softap_get_config(&config);
	wifi_get_ip_info(SOFTAP_IF, &ip);
	os_printf("AP SSID: %s\n", config.ssid);
	os_printf("AP PW: %s\n", config.password);
	os_printf("AP AUTH: %s\n", auth_mode_str(config.authmode));
	os_printf("AP IP: " IPSTR "\n", IP2STR(&ip.ip));
}

static void ICACHE_FLASH_ATTR
print_sta_config(void)
{
	struct station_config config = {0};
	struct ip_info ip = {0};

	wifi_station_get_config(&config);
	wifi_get_ip_info(STATION_IF, &ip);
	os_printf("STA SSID: %s\n", config.ssid);
	os_printf("STA PW: %s\n", config.password);
	os_printf("STA IP: " IPSTR "\n", IP2STR(&ip.ip));
}

static void ICACHE_FLASH_ATTR
init_done_cb(void)
{
	os_printf("init_done_cb\r\n");

	print_ap_config();
	print_sta_config();

	cdnsd_init(IPADDR_ANY, DNS_SERVER_PORT);
	httpd_init(IPADDR_ANY, HTTP_SERVER_PORT);
}

static void ICACHE_FLASH_ATTR
event_handler_cb(System_Event_t *sev)
{
	os_printf("event_handler_cb(evt=%d)\n", sev->event);
	switch (sev->event) {
	case EVENT_STAMODE_CONNECTED:
		os_printf("  sta connected to ap (chan=%d, bssid=%02x:%02x:%02x:%02x:%02x:%02x)\n",
			sev->event_info.connected.channel,
			sev->event_info.connected.bssid[0],
			sev->event_info.connected.bssid[1],
			sev->event_info.connected.bssid[2],
			sev->event_info.connected.bssid[3],
			sev->event_info.connected.bssid[4],
			sev->event_info.connected.bssid[5]);
		break;
	case EVENT_STAMODE_DISCONNECTED:
		os_printf("  sta disconnected from ap (reason=%d)\n",
			sev->event_info.disconnected.reason);
		break;
	case EVENT_STAMODE_AUTHMODE_CHANGE:
		os_printf("  sta auth mode %d -> %d\n",
			sev->event_info.auth_change.old_mode,
			sev->event_info.auth_change.new_mode);
		break;
	case EVENT_STAMODE_GOT_IP:
		os_printf("  sta got ip: 0x%08x\n",
			sev->event_info.got_ip.ip.addr);
		break;
	case EVENT_SOFTAPMODE_STACONNECTED:
		os_printf("  ap sta connected aid=%d mac=%02x:%02x:%02x:%02x:%02x:%02x\n",
			sev->event_info.sta_connected.aid,
			sev->event_info.sta_connected.mac[0],
			sev->event_info.sta_connected.mac[1],
			sev->event_info.sta_connected.mac[2],
			sev->event_info.sta_connected.mac[3],
			sev->event_info.sta_connected.mac[4],
			sev->event_info.sta_connected.mac[5]);
		break;
	case EVENT_SOFTAPMODE_STADISCONNECTED:
		os_printf("  ap sta disconnected aid=%d mac=%02x:%02x:%02x:%02x:%02x:%02x\n",
			sev->event_info.sta_disconnected.aid,
			sev->event_info.sta_disconnected.mac[0],
			sev->event_info.sta_disconnected.mac[1],
			sev->event_info.sta_disconnected.mac[2],
			sev->event_info.sta_disconnected.mac[3],
			sev->event_info.sta_disconnected.mac[4],
			sev->event_info.sta_disconnected.mac[5]);
		break;
	case EVENT_SOFTAPMODE_PROBEREQRECVED:
		os_printf("  ap probe req rssi=%d mac=%02x:%02x:%02x:%02x:%02x:%02x\n",
			sev->event_info.ap_probereqrecved.rssi,
			sev->event_info.ap_probereqrecved.mac[0],
			sev->event_info.ap_probereqrecved.mac[1],
			sev->event_info.ap_probereqrecved.mac[2],
			sev->event_info.ap_probereqrecved.mac[3],
			sev->event_info.ap_probereqrecved.mac[4],
			sev->event_info.ap_probereqrecved.mac[5]);
		break;
	case EVENT_OPMODE_CHANGED:
		os_printf("  opmode changed %d -> %d\n",
			sev->event_info.opmode_changed.old_opmode,
			sev->event_info.opmode_changed.new_opmode);
		break;
	case EVENT_SOFTAPMODE_DISTRIBUTE_STA_IP:
		os_printf("  ap new sta ip: aid=%d ip=0x%08x\n",
			sev->event_info.distribute_sta_ip.ip.addr,
			sev->event_info.distribute_sta_ip.aid);
		break;
	}
}

void ICACHE_FLASH_ATTR
user_init(void)
{
	os_printf("@74880 compile time:"__DATE__" "__TIME__"\r\n");
	uart_div_modify(0, UART_CLK_FREQ / 115200);
	os_printf("@115200 compile time:"__DATE__" "__TIME__"\r\n");

	/* Stop DHCP from offering a default route in AP mode */
	bool offer_router = false;
	wifi_softap_set_dhcps_offer_option(OFFER_ROUTER, &offer_router);

	/* wifi_softap_set_config(&config); */

	/* Start in AP+Station mode */
	wifi_set_opmode(STATIONAP_MODE);

	system_init_done_cb(init_done_cb);
	wifi_set_event_handler_cb(event_handler_cb);

	system_show_malloc();
}

void ICACHE_FLASH_ATTR
user_pre_init(void)
{
	const partition_item_t* partition_table = at_partition_table;
	uint32 partition_table_size = sizeof(at_partition_table)/sizeof(at_partition_table[0]);

	os_printf("user_pre_init\n");
	if(!system_partition_table_regist(partition_table, partition_table_size,SPI_FLASH_SIZE_MAP)) {
		os_printf("system_partition_table_regist fail\n");
	}
}

#include <stdint.h>
#include "c_types.h"
#include "osapi.h"

#include "relib/esp8266.h"
#include "lwip/sys.h"

u32_t sys_now() {
	return WDEV_TIMER->COUNT / 1000;
}

struct ip_info;

void ICACHE_FLASH_ATTR
dhcps_start(struct ip_info *info)
{
	os_printf("dhcps_start()\n");
}

void ICACHE_FLASH_ATTR
dhcps_stop(void)
{
	os_printf("dhcps_stop()\n");
}

bool ICACHE_FLASH_ATTR
wifi_softap_set_dhcps_offer_option(uint8_t level, void* optarg)
{
	os_printf("wifi_softap_set_dhcps_offer_option()\n");
}

void ICACHE_FLASH_ATTR
espconn_init(void)
{
	os_printf("espconn_init()\n");
}


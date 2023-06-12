#include <stdint.h>
#include "c_types.h"
#include "osapi.h"

#include "relib/esp8266.h"
#include "lwip/sys.h"
#include "lwip/pbuf.h"
#include "lwip/netif.h"

/* Note:
 * wpa_supplicant_send_4_of_4 ORs pbuf->flags with 0x40
 * wpa_supplicant_send_2_of_2 ORs pbuf->flags with 0x80
 * Fortunately the highest in-use flag for lwip2 is 0x20 currently (PBUF_FLAG_TCP_FIN)
 * Also it doesn't seem to read the flag value, only updates it...
 */

static_assert(OFFSET_OF(struct pbuf, ref) == 14, "pbuf ref offset mismatch");
static_assert(OFFSET_OF(struct pbuf, if_idx) == 15, "pbuf if_idx offset mismatch");
static_assert(OFFSET_OF(struct pbuf, eb) == 16, "pbuf eb offset mismatch");

static_assert(OFFSET_OF(struct netif, gw) == 12, "netif eb offset mismatch");
static_assert(OFFSET_OF(struct netif, linkoutput) == 24, "netif linkoutput offset mismatch");
static_assert(OFFSET_OF(struct netif, state) == 28, "netif state offset mismatch");
static_assert(OFFSET_OF(struct netif, hostname) == 44, "netif hostname offset mismatch");
static_assert(OFFSET_OF(struct netif, mtu) == 48, "netif mtu offset mismatch");
static_assert(OFFSET_OF(struct netif, hwaddr_len) == 50, "netif hwaddr_len offset mismatch");
static_assert(OFFSET_OF(struct netif, hwaddr) == 51, "netif hwaddr offset mismatch");
static_assert(OFFSET_OF(struct netif, flags) == 57, "netif flags offset mismatch");
static_assert(OFFSET_OF(struct netif, name) == 58, "netif name offset mismatch");
static_assert(OFFSET_OF(struct netif, num) == 60, "netif num offset mismatch");

struct ip_info;

typedef enum {
  LWIP1_PBUF_TRANSPORT,
  LWIP1_PBUF_IP,
  LWIP1_PBUF_LINK,
  LWIP1_PBUF_RAW
} lwip1_pbuf_layer;


typedef enum {
  LWIP1_PBUF_RAM, /* pbuf data is stored in RAM */
  LWIP1_PBUF_ROM, /* pbuf data is stored in ROM */
  LWIP1_PBUF_REF, /* pbuf comes from the pbuf pool */
  LWIP1_PBUF_POOL, /* pbuf payload refers to RAM */
  LWIP1_PBUF_ESF_RX /* pbuf payload is from WLAN */
} lwip1_pbuf_type;

struct pbuf* lwip1_pbuf_alloc(lwip1_pbuf_layer l, uint16_t length, lwip1_pbuf_type type);
uint8_t lwip1_pbuf_free(struct pbuf *p);

int system_pp_recycle_rx_pkt(void *eb); /* trampoline/alias which calls ppRecycleRxPkt */

u32_t sys_now() {
	return WDEV_TIMER->COUNT / 1000;
}

struct pbuf* ICACHE_FLASH_ATTR
lwip1_pbuf_alloc(lwip1_pbuf_layer l, uint16_t length, lwip1_pbuf_type type)
{
	/* Both layer and type enums are different in LWIP2, so we need to translate.
	 * Furtunately they are not read in the ETS code */
	pbuf_layer lwip2_l = PBUF_RAW;
	pbuf_type lwip2_type = PBUF_RAM;
	switch (l) {
	case LWIP1_PBUF_TRANSPORT: lwip2_type = PBUF_TRANSPORT; break;
	case LWIP1_PBUF_IP: lwip2_type = PBUF_IP; break;
	case LWIP1_PBUF_LINK: lwip2_type = PBUF_LINK; break;
	case LWIP1_PBUF_RAW: lwip2_type = PBUF_RAW; break;
	}
	switch (type) {
	case LWIP1_PBUF_RAM:    lwip2_l = PBUF_RAM; break;
	case LWIP1_PBUF_ROM:    lwip2_l = PBUF_ROM; break;
	case LWIP1_PBUF_REF:    lwip2_l = PBUF_REF; break;
	case LWIP1_PBUF_POOL:   lwip2_l = PBUF_POOL; break;
	case LWIP1_PBUF_ESF_RX: lwip2_l = PBUF_REF; break;
	}
	return pbuf_alloc(lwip2_l, length, lwip2_type);
}

uint8_t ICACHE_FLASH_ATTR
lwip1_pbuf_free(struct pbuf *p)
{
	if (p->eb) {
		system_pp_recycle_rx_pkt(p->eb);
		p->eb = NULL;
	}
	return pbuf_free(p);
}

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



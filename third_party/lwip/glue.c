#include <stdint.h>
#include "c_types.h"
#include "osapi.h"

#include "lwip/sys.h"
#include "lwip/pbuf.h"
#include "lwip/netif.h"

struct pbuf* ICACHE_FLASH_ATTR
lwip1_pbuf_alloc(pbuf_layer l, uint16_t length, pbuf_type type)
{
	return pbuf_alloc(l, length, type);
}

uint8_t ICACHE_FLASH_ATTR
lwip1_pbuf_free(struct pbuf *p)
{
	return pbuf_free(p);
}

#define MEM_DEFAULT_USE_DRAM 1

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "c_types.h"
#include "ets_sys.h"
#include "mem.h"
#include "lwip/ip_addr.h"
#include "lwip/err.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"

#include "relib/relib.h"
#include "relib/s/ieee80211_conn.h"
#include "relib/s/ieee80211com.h"

#if LIBLWIP_VER == 2
struct ip_addr {
	ip4_addr_t addr;
};

struct ip_info {
	ip4_addr_t ip;
	ip4_addr_t netmask;
	ip4_addr_t gw;
};
#define IP_ADDR_T const ip4_addr_t
#define IPSTR "%d.%d.%d.%d"
#define IP2STR(ipaddr) ip4_addr1_16(ipaddr), \
    ip4_addr2_16(ipaddr), \
    ip4_addr3_16(ipaddr), \
    ip4_addr4_16(ipaddr)
#else
#define IP_ADDR_T ip_addr_t
#endif

#include "lwip/app/dhcpserver.h"

#define QUEUE_LEN 10
#define OFFSET_OF(s, f) __builtin_offsetof(s, f)

#define LWIP_IF0_TASK_NUM 28
#define LWIP_IF1_TASK_NUM 29

static_assert(sizeof(ETSEvent) * QUEUE_LEN == 80, "event queue size mismatch");

enum dhcp_status {
	DHCP_STOPPED,
	DHCP_STARTED
};

typedef struct ip_info ip_info_st;

err_t etharp_output(struct netif *netif, struct pbuf *q, IP_ADDR_T *ipaddr); /* netif_output_fn */
err_t ethernet_input(struct pbuf *p, struct netif *netif);
err_t ieee80211_output_pbuf(struct netif *netif, struct pbuf *p);
int system_pp_recycle_rx_pkt(void *eb);
void wifi_station_dhcpc_event(void);  /* dhcp_event_fn */
enum dhcp_status wifi_station_dhcpc_status(void);
void wifi_station_set_default_hostname(uint8_t *macaddr);
void ets_task(ETSTask task, int prio, ETSEvent *queue, uint8_t qlen);

extern uint8_t dhcps_flag; /* user_interface.o */
extern ieee80211com_st g_ic; /* defined in ieee80211.c */

static ETSEvent lwipIf0EvtQueue[QUEUE_LEN];
static ETSEvent lwipIf1EvtQueue[QUEUE_LEN];
char *hostname;
bool default_hostname;

static const char mem_debug_file[] ICACHE_RODATA_ATTR = __FILE__;


struct netif* ICACHE_FLASH_ATTR
eagle_lwip_getif(int i)
{
	if (i > 1 || g_ic.ic_ifx_conn[i] == NULL) {
		return NULL;
	}
	return g_ic.ic_ifx_conn[i]->ni_ifp;
}

static void ICACHE_FLASH_ATTR
ifx_input(ETSEvent *e, int n)
{
	if (e->sig != 0) {
		return;
	}

	struct netif *myif = eagle_lwip_getif(n);
	if(myif != NULL) {
		myif->input((struct pbuf *)e->par, myif);
	}
	else {
		struct pbuf *p = (struct pbuf *)e->par;
		system_pp_recycle_rx_pkt(p->eb);
		pbuf_free(p);
	}
}

void ICACHE_FLASH_ATTR
lwip_if0_task(ETSEvent *e)
{
	ifx_input(e, 0);
}

void ICACHE_FLASH_ATTR
lwip_if1_task(ETSEvent *e)
{
	ifx_input(e, 1);
}


err_t ICACHE_FLASH_ATTR
eagle_low_level_init(struct netif *netif)  /* LWIP1 netif_init_fn */
{
	netif->hwaddr_len = 6;
	netif->mtu = 1500;
	netif->flags = NETIF_FLAG_IGMP | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP | NETIF_FLAG_BROADCAST;
	return ERR_OK;
}

err_t ICACHE_FLASH_ATTR
low_level_output(struct netif *netif, struct pbuf *p)  /* LWIP1  netif_linkoutput_fn */
{
	err_t ret;
	struct pbuf *p_ram;

	if (p->next == (struct pbuf *)0x0) {
		/* fast-path for unfragmented buffer */
		return ieee80211_output_pbuf(netif, p);
	}

	p_ram = pbuf_alloc(PBUF_TRANSPORT ,p->tot_len, PBUF_RAM);
	if (p_ram == (struct pbuf *)0x0) {
		ret = ERR_MEM;
	}
	else {
		/* defragment pbuf list into single ram pbuf */
		pbuf_copy(p_ram, p);
		ret = ieee80211_output_pbuf(netif, p_ram);
		pbuf_free(p_ram);
	}

	return ret;
}

void ICACHE_FLASH_ATTR
eagle_lwip_if_free(ieee80211_conn_st *conn)
{
	if (conn->opmode == IEEE80211_M_STA) {
		netif_remove(conn->ni_ifp);
	}
	else {
		if (dhcps_flag) {
			dhcps_stop();
		}
		netif_remove(conn->ni_ifp);
	}
	if (conn->ni_ifp != NULL) {
		os_free(conn->ni_ifp);
		conn->ni_ifp = NULL;
	}
}

static void ICACHE_FLASH_ATTR
netif_set_dhcp_event_cb(struct netif *netif)
{
#if LIBLWIP_VER == 1
	netif->dhcp_event = wifi_station_dhcpc_event;
#else
	/* TODO */
#endif
}

struct netif* ICACHE_FLASH_ATTR
eagle_lwip_if_alloc(ieee80211_conn_st *conn, uint8_t *macaddr, ip_info_st *ipinfo)
{
	struct netif *netif = conn->ni_ifp;
	if (netif == NULL) {
		netif = (void *)os_zalloc(sizeof(*netif));
		if (netif == NULL) {
			return NULL;
		}
		conn->ni_ifp = netif;
	}

	char *host = NULL;
	if (conn->opmode == IEEE80211_M_STA) {
		if (default_hostname) {
			wifi_station_set_default_hostname(macaddr);
		}
		host = hostname;
	}

	netif->hostname = host;
	netif->state = conn;
	netif->name[0] = 'e';  /* ew => e[agle] w[ireless]? */
	netif->name[1] = 'w';
	netif->output = etharp_output;
	netif->linkoutput = ieee80211_output_pbuf;
	memcpy(netif->hwaddr, macaddr, 6);

	ip_info_st ipi = *ipinfo;
	if (conn->opmode == IEEE80211_M_STA) {
		netif_set_dhcp_event_cb(netif);
		if (wifi_station_dhcpc_status() == DHCP_STARTED) {
			memset(&ipi, 0, sizeof(ipi));
		}
		ets_task(lwip_if0_task, LWIP_IF0_TASK_NUM, lwipIf0EvtQueue, QUEUE_LEN);
		netif_add(netif, &ipi.ip, &ipi.netmask, &ipi.gw, conn, eagle_low_level_init, ethernet_input);
	}
	else {
		netif_set_addr(netif, &ipi.ip, &ipi.netmask, &ipi.gw);
		ets_task(lwip_if1_task, LWIP_IF1_TASK_NUM, lwipIf1EvtQueue, QUEUE_LEN);
		netif_add(netif, &ipi.ip, &ipi.netmask, &ipi.gw, conn, eagle_low_level_init, ethernet_input);
		if (dhcps_flag != false) {
			dhcps_start(&ipi);
			os_printf_plus("dhcp server start:(");
			os_printf_plus("ip:" IPSTR ",mask:" IPSTR ",gw:" IPSTR,
				IP2STR((&ipi.ip)), IP2STR((&ipi.netmask)), IP2STR((&ipi.gw)));
			os_printf_plus(")\n");
		}
	}

	return netif;
}

/*
void wifi_station_set_default_hostname(uint8_t *macaddr)

{
  undefined auStack_30 [32];
  uint8_t *puStack_10;
  
  puStack_10 = macaddr;
  if (hostname != (char *)0x0) {
    vPortFree(hostname,"user_interface.c",0xb5b);
    hostname = (char *)0x0;
  }
  hostname = (char *)pvPortMalloc(0x20,"user_interface.c",0xb5f,true);
  if (hostname != (char *)0x0) {
    system_get_string_from_flash("ESP-%02X%02X%02X",auStack_30,0x14);
    ets_sprintf(hostname,auStack_30,puStack_10[3],puStack_10[4],puStack_10[5]);
  }
  return;
}
*/

/* syms:
         U dhcps_flag
         U dhcps_start
         U dhcps_stop
         U etharp_output
         U ethernet_input
         U ets_memcpy
         U ets_task
         U g_ic
         U ieee80211_output_pbuf
         U netif_add
         U netif_remove
         U netif_set_addr
         U os_printf_plus
         U pbuf_alloc
         U pbuf_copy
         U pbuf_free
         U ppRecycleRxPkt
         U pvPortMalloc
         U vPortFree
         U wifi_station_dhcpc_event
         U wifi_station_dhcpc_status
         U wifi_station_set_default_hostname
00000000 D default_hostname
00000000 B hostname
00000000 t mem_debug_file_82
00000004 b lwipIf0EvtQueue_90
00000008 t lwip_if0_task
00000008 b lwipIf1EvtQueue_95
00000010 t flash_str$6225_56_15
00000014 t low_level_output
00000030 t flash_str$6227_56_16
0000004c t lwip_if1_task
00000060 t flash_str$6228_56_17
00000088 t eagle_low_level_init
00000138 T eagle_lwip_if_alloc
00000340 T eagle_lwip_if_free
000003c8 T eagle_lwip_getif
*/

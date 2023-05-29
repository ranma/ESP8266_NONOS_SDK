#include <stdint.h>
#include "c_types.h" /* for ICACHE_FLASH_ATTR */
#include "mem.h"
#include "osapi.h"
#include "relib/s/lwip1_pbuf.h"

#define DPRINT(msg) do { \
	uint32_t caller = (uint32_t)__builtin_return_address(0); \
	os_printf("[@%08x] %s", caller, msg); \
} while (0)

#define DPRINTF(fmt, ...) do { \
	uint32_t caller = (uint32_t)__builtin_return_address(0); \
	os_printf("[@%08x] " fmt, caller, __VA_ARGS__); \
} while (0)

#define ONCE(x) do { \
	static int once = 0; \
	if (!once) { \
		once = 1; \
		x; \
	} \
} while (0)

#define NTOHS(x) \
	((((x) & 0xff00) >> 8) \
	|(((x) & 0x00ff) << 8))

#define NTOHL(x) \
	(NTOHS(((x) & 0xffff0000) >> 16) \
	|(NTOHS((x) & 0x0000ffff) << 16))

#define ERR_OK          0    /* No error, everything OK. */

/** Whether the network interface is 'up'. This is
 * a software flag used to control whether this network
 * interface is enabled and processes traffic.
 * It is set by the startup code (for static IP configuration) or
 * by dhcp/autoip when an address has been assigned.
 */
#define NETIF_FLAG_UP           0x01U
/** If set, the netif has broadcast capability.
 * Set by the netif driver in its init function. */
#define NETIF_FLAG_BROADCAST    0x02U
/** If set, the netif is one end of a point-to-point connection.
 * Set by the netif driver in its init function. */
#define NETIF_FLAG_POINTTOPOINT 0x04U
/** If set, the interface is configured using DHCP.
 * Set by the DHCP code when starting or stopping DHCP. */
#define NETIF_FLAG_DHCP         0x08U
/** If set, the interface has an active link
 *  (set by the network interface driver).
 * Either set by the netif driver in its init function (if the link
 * is up at that time) or at a later point once the link comes up
 * (if link detection is supported by the hardware). */
#define NETIF_FLAG_LINK_UP      0x10U
/** If set, the netif is an ethernet device using ARP.
 * Set by the netif driver in its init function.
 * Used to check input packet types and use of DHCP. */
#define NETIF_FLAG_ETHARP       0x20U
/** If set, the netif is an ethernet device. It might not use
 * ARP or TCP/IP if it is used for PPPoE only.
 */
#define NETIF_FLAG_ETHERNET     0x40U
/** If set, the netif has IGMP capability.
 * Set by the netif driver in its init function. */
#define NETIF_FLAG_IGMP         0x80U

/*
 * 1. LINK_HLEN 14Byte will be remove in WLAN layer
 * 2. IEEE80211_HDR_MAX_LEN needs 40 bytes.
 * 3. encryption needs exra 4 bytes ahead of actual data payload, and require
 *     DAddr and SAddr to be 4-byte aligned.
 * 4. TRANSPORT and IP are all 20, 4 bytes aligned, nice...
 * 5. LCC add 6 bytes more, We don't consider WAPI yet...
 * 6. define LWIP_MEM_ALIGN to be 4 Byte aligned, pbuf struct is 16B, Only thing may be
 *     matter is ether_hdr is not 4B aligned.
 *
 * So, we need extra (40 + 4 - 14) = 30 and it's happen to be 4-Byte aligned
 *
 *    1. lwip
 *         | empty 30B    | eth_hdr (14B)  | payload ...|
 *              total: 44B ahead payload
 *    2. net80211
 *         | max 80211 hdr, 32B | ccmp/tkip iv (8B) | sec rsv(4B) | payload ...|
 *              total: 40B ahead sec_rsv and 44B ahead payload
 *
 */
#define EP_OFFSET 36

/*
 * 9632 Byte of static RX buffer @_wdev_rx_mblk_space
 * 80 Byte of static RX dma descriptors @_wdev_rx_desc_space
 * Since a descriptor is 3 32bit word (12 bytes), we can
 * fit 6 descriptors into those 80 bytes, giving about 1605 bytes
 * per RX buffer.
 *
 * Additionally, looking at received payload pointers we can
 * deduce that the actual raw RX buffer size is 1604 bytes, as
 * that is the granularity of the pointer differnces, e.g.:
 * 3FFEA67E-3FFEA03A => 1604
 * 3FFEACC2-3FFEA03A => 3208
 * 3FFEB94A-3FFEA03A => 6416
 *
 * While the code comment says the MAC header is 32bytes, that only
 * seems to be true for 802.11abg dn for 802.11n it is 36bytes.
 * Together with the WPA2+CCMP 16 byte encryption header this
 * would then exactly match the presumed 50 bytes.
 *
 * Also it looks like the maximum MTU for 802.11 without headers
 * is 2304 bytes, but there pretty much isn't enough space for
 * that in this setup.
 */

typedef enum {
  PBUF_TRANSPORT,
  PBUF_IP,
  PBUF_LINK,
  PBUF_RAW
} pbuf_layer;

typedef enum {
  PBUF_RAM, /* pbuf data is stored in RAM */
  PBUF_ROM, /* pbuf data is stored in ROM */
  PBUF_REF, /* pbuf comes from the pbuf pool */
  PBUF_POOL, /* pbuf payload refers to RAM */
  PBUF_ESF_RX /* pbuf payload is from WLAN */
} pbuf_type;

typedef int err_t;

struct pbuf* ICACHE_FLASH_ATTR
pbuf_alloc(pbuf_layer l, uint16_t length, pbuf_type type)
{
	DPRINTF("pbuf_alloc(%s, %d, %s)\n",
		l == PBUF_RAW ? "raw" : "?",
		length,
		type == PBUF_RAM ? "ram" : (
			type == PBUF_REF ? "ref" : "?"));

	int offset = 0;
	if (l == PBUF_RAW) {
		offset += EP_OFFSET;
	}

	struct pbuf *p;
	if (type == PBUF_RAM) {
		/* Note: Should be aligned, which is the case for EP_OFFSET,
		 * but may not be the case for other layer types.
		 * Here we only handle PBUF_RAW, so we don't bother */
		p = (void*)os_malloc_dram(sizeof(*p) + offset + length);
		p->payload = ((uint8_t*)&p[1]) + offset;
	} else {
		p = (void*)os_malloc_dram(sizeof(*p));
		p->payload = NULL;
	}
	p->next = NULL;
	p->eb = NULL;
	p->flags = 0;
	p->len = p->tot_len = length;
	p->type = type;
	p->ref = 1;
	os_printf("p=%p payload=%p\n", p, p->payload);
	return p;
}

int system_pp_recycle_rx_pkt(void *eb);

uint8_t ICACHE_FLASH_ATTR
pbuf_free(struct pbuf *p)
{
	if (--p->ref == 0) {
		if (p->eb) {
			system_pp_recycle_rx_pkt(p->eb);
		}
		os_free((void*)p);
	}
}

err_t ICACHE_FLASH_ATTR
pbuf_copy(struct pbuf *p_to, struct pbuf *p_from)
{
	DPRINTF("pbuf_copy(%p, %p)\n", p_to, p_from);
}


void ICACHE_FLASH_ATTR
pbuf_ref(struct pbuf *p)
{
	p->ref++;
	DPRINT("pbuf_ref()\n");
}

struct ip_addr {
	uint32_t addr;
};

struct dhcp;
struct udp_pcb;
struct netif;
typedef struct ip_addr ip_addr_t;

/** Function prototype for netif init functions. Set up flags and output/linkoutput
 * callback functions in this function.
 */
typedef err_t (*netif_init_fn)(struct netif *netif);

/** Function prototype for netif->input functions. This function is saved as 'input'
 * callback function in the netif struct. Call it when a packet has been received.
 *
 * @param p The received packet, copied into a pbuf
 * @param inp The netif which received the packet
 */
typedef err_t (*netif_input_fn)(struct pbuf *p, struct netif *inp);

/** Function prototype for netif->output functions. Called by lwIP when a packet
 * shall be sent. For ethernet netif, set this to 'etharp_output' and set
 * 'linkoutput'.
 *
 * @param netif The netif which shall send a packet
 * @param p The packet to send (p->payload points to IP header)
 * @param ipaddr The IP address to which the packet shall be sent
 */
typedef err_t (*netif_output_fn)(struct netif *netif, struct pbuf *p,
       ip_addr_t *ipaddr);

/** Function prototype for netif->linkoutput functions. Only used for ethernet
 * netifs. This function is called by ARP when a packet shall be sent.
 *
 * @param netif The netif which shall send a packet
 * @param p The packet to send (raw ethernet packet)
 */
typedef err_t (*netif_linkoutput_fn)(struct netif *netif, struct pbuf *p);

/*add DHCP event processing by LiuHan*/
typedef void (*dhcp_event_fn)(void);

/** Function prototype for netif igmp_mac_filter functions */
typedef err_t (*netif_igmp_mac_filter_fn)(struct netif *netif,
       ip_addr_t *group, uint8_t action);

struct netif {
	struct netif *next; /** pointer to next in linked list */
	ip_addr_t ip_addr;
	ip_addr_t netmask;
	ip_addr_t gw;
	/** This function is called by the network device driver
	 *	to pass a packet up the TCP/IP stack. 向IP层输入数据包*/
	netif_input_fn input;
	/** This function is called by the IP module when it wants
	 *	to send a packet on the interface. This function typically
	 *	first resolves the hardware address, then sends the packet. 发送IP数据包*/
	netif_output_fn output;
	/** This function is called by the ARP module when it wants
	 *	to send a packet on the interface. This function outputs
	 *	the pbuf as-is on the link medium. 底层数据包发送*/
	netif_linkoutput_fn linkoutput;
	/** This field can be set by the device driver and could point
	 *	to state information for the device. 自由设置字段，比如指向底层设备相关信息*/
	void *state;
	/** the DHCP client state information for this netif */
	struct dhcp *dhcp;
	struct udp_pcb *dhcps_pcb;	//dhcps
	dhcp_event_fn dhcp_event;
	/* the hostname for this netif, NULL is a valid value */
	char*	hostname;
	/** maximum transfer unit (in bytes) 该接口允许的最大数据包长度，多是1500*/
	uint16_t mtu;
	/** number of bytes used in hwaddr该接口物理地址长度 */
	uint8_t hwaddr_len;
	/** link level hardware address of this interface 该接口物理地址*/
	uint8_t hwaddr[6];
	/** flags (see NETIF_FLAG_ above) 该接口状态、属性字段*/
	uint8_t flags;
	/** descriptive abbreviation 该接口的名字*/
	char name[2];
	/** number of this interface 该接口的编号*/
	uint8_t num;
	/** This function could be called to add or delete a entry in the multicast
			filter table of the ethernet MAC.*/
	netif_igmp_mac_filter_fn igmp_mac_filter;
};

/** The default network interface. */
struct netif* netif_default;  /* Used in wifi_get_broadcast_if, but not dereferenced */

void ICACHE_FLASH_ATTR
netif_set_up(struct netif *netif)
{
	DPRINTF("netif_set_up(%p)\n", netif);
	netif->flags |= NETIF_FLAG_UP;
}

void ICACHE_FLASH_ATTR
netif_set_down(struct netif *netif)
{
	DPRINTF("netif_set_down(%p)\n", netif);
	netif->flags &= ~NETIF_FLAG_UP;
}

void ICACHE_FLASH_ATTR
netif_set_default(struct netif *netif)
{
	DPRINTF("netif_set_default(%p)\n", netif);
	netif_default = netif;
	/* Without this, esp8266 doesn't reach "ASSOCIATED" state (3)
	 see comment on netif_default above.
	 e.g. non-working [0 == INIT, 2 == AUTH, 3 == ASSOCIATED, 5 == RUN from BSD IEEE80211 stack states]]
scandone
state: 0 -> 2 (b0)
state: 2 -> 0 (2)
reconnect
scandone
state: 0 -> 2 (b0)
state: 2 -> 0 (2)
reconnect

	vs. working:
scandone
state: 0 -> 2 (b0)
state: 2 -> 3 (0)
state: 3 -> 5 (10)

	well, either this or it was because of bzero() in netif_add...
	*/
}

void ICACHE_FLASH_ATTR
netif_set_addr(struct netif *netif, ip_addr_t *ipaddr, ip_addr_t *netmask, ip_addr_t *gw)
{
	DPRINTF("netif_set_addr(%p, %08x, %08x, %08x)\n",
		netif, ipaddr->addr, netmask->addr, gw->addr);
	netif->ip_addr.addr = ipaddr->addr;
	netif->netmask.addr = netmask->addr;
	netif->gw.addr = gw->addr;
}

static int netifnum;
static struct netif* netif_list;

struct netif* ICACHE_FLASH_ATTR
netif_add(struct netif *netif, ip_addr_t *ipaddr, ip_addr_t *netmask, ip_addr_t *gw, void *state, netif_init_fn init, netif_input_fn input)
{
	DPRINTF("netif_add(%p)\n", netif);
	netif->flags = 0;
	netif->dhcp = NULL;
	netif->dhcps_pcb = NULL;
	netif->igmp_mac_filter = NULL;
#if 0
	netif_set_addr(netif, ipaddr, netmask, gw);
#else
	netif->ip_addr.addr = ipaddr->addr;
	netif->netmask.addr = netmask->addr;
	netif->gw.addr = gw->addr;
#endif
	netif->state = state;
	netif->input = input;
	netif->num = netifnum++;
	if (init(netif) != ERR_OK) {
		return NULL;
	}
	/* add this netif to the list */
	netif->next = netif_list;
	netif_list = netif;
	return netif;
}

void ICACHE_FLASH_ATTR
netif_remove(struct netif * netif)
{
	DPRINT("netif_remove()\n");
}

void ICACHE_FLASH_ATTR
dhcp_cleanup(struct netif *netif)
{
	DPRINT("dhcp_cleanup()\n");
}

err_t ICACHE_FLASH_ATTR
dhcp_start(struct netif *netif)
{
	DPRINT("dhcp_start()\n");
}

err_t ICACHE_FLASH_ATTR
dhcp_release(struct netif *netif)
{
	DPRINT("dhcp_release()\n");
}

void ICACHE_FLASH_ATTR
dhcp_stop(struct netif *netif)
{
	DPRINT("dhcp_stop()\n");
}

struct ip_info;

void ICACHE_FLASH_ATTR
dhcps_start(struct ip_info *info)
{
	DPRINT("dhcps_start()\n");
}

void ICACHE_FLASH_ATTR
dhcps_stop(void)
{
	DPRINT("dhcps_stop()\n");
}

bool ICACHE_FLASH_ATTR
wifi_softap_set_dhcps_offer_option(uint8_t level, void* optarg)
{
	DPRINT("wifi_softap_set_dhcps_offer_option()\n");
}

void ICACHE_FLASH_ATTR
espconn_init(void)
{
	DPRINT("espconn_init()\n");
}

void ICACHE_FLASH_ATTR
lwip_init(void)
{
	DPRINT("lwip_init()\n");
}

void ICACHE_FLASH_ATTR
sys_check_timeouts(void)
{
	ONCE(DPRINT("sys_check_timeouts()\n"));
}

/* netif linkoutput fn should be set to this callback, referenced in
 * eagle_lwip_if.o  */
err_t ICACHE_FLASH_ATTR
etharp_output(struct netif *netif, struct pbuf *q, ip_addr_t *ipaddr)
{
	ONCE(DPRINT("etharp_output()\n"));
	/* in turn this calls netif->linkoutput() */
}

/**
 * Process received ethernet frames. Using this function instead of directly
 * calling ip_input and passing ARP frames through etharp in ethernetif_input,
 * the ARP cache is protected from concurrent access.
 *
 * @param p the recevied packet, p->payload pointing to the ethernet header
 * @param netif the network interface on which the packet was received
 */
err_t ICACHE_FLASH_ATTR
ethernet_input(struct pbuf *p, struct netif *netif)
{
	uint8_t *data = p->payload;
	int i;
	DPRINTF("ethernet_input(len=%d, payload=%p)\n", p->len, p->payload);
	for (i=0; i<40 && i<p->len; i++) {
		if (i == 6 || i == 12 || i == 14) {
			os_printf(" ");
		}
		os_printf("%02x", *data++);
	}
	os_printf("\n");
	pbuf_free(p);
	return ERR_OK;
}

struct eth_addr {
	uint8_t addr[6];
} __attribute__((packed));
const struct eth_addr ethbroadcast = {{0xff,0xff,0xff,0xff,0xff,0xff}}; /* refernced by ieee80211.o */

/*
../lib/weak/libpp.a(pp.o): In function `ppRecycleRxPkt':
/home/wchen/gwen/nonos/app/pp/pp.c:758: undefined reference to `pbuf_free'
../lib/weak/libnet80211.a(ieee80211_hostap.o): In function `ieee80211_hostap_attach':
/home/wchen/gwen/nonos/app/net80211/ieee80211_hostap.c:535: undefined reference to `pbuf_alloc'
../lib/weak/libnet80211.a(ieee80211_hostap.o): In function `hostap_deliver_data':
/home/wchen/gwen/nonos/app/net80211/ieee80211_hostap.c:546: undefined reference to `pbuf_alloc'
../lib/weak/libnet80211.a(ieee80211_hostap.o): In function `hostap_handle_timer':
/home/wchen/gwen/nonos/app/net80211/ieee80211_hostap.c:952: undefined reference to `pbuf_free'
../lib/weak/libnet80211.a(ieee80211_hostap.o): In function `hostap_input':
/home/wchen/gwen/nonos/app/net80211/ieee80211_hostap.c:1359: undefined reference to `pbuf_alloc'
/home/wchen/gwen/nonos/app/net80211/ieee80211_hostap.c:1389: undefined reference to `pbuf_free'
../lib/weak/libnet80211.a(ieee80211_hostap.o): In function `ApFreqCalTimerCB':
/home/wchen/gwen/nonos/app/net80211/ieee80211_hostap.c:3274: undefined reference to `netif_set_up'
/home/wchen/gwen/nonos/app/net80211/ieee80211_hostap.c:3277: undefined reference to `netif_set_default'
../lib/weak/libnet80211.a(ieee80211_hostap.o): In function `wifi_softap_start':
/home/wchen/gwen/nonos/app/net80211/ieee80211_hostap.c:3376: undefined reference to `netif_set_up'
/home/wchen/gwen/nonos/app/net80211/ieee80211_hostap.c:3378: undefined reference to `netif_set_default'
/home/wchen/gwen/nonos/app/net80211/ieee80211_hostap.c:3339: undefined reference to `netif_set_down'
../lib/weak/libnet80211.a(ieee80211_hostap.o):(.text.wifi_softap_stop+0x99): undefined reference to `netif_set_down'
../lib/weak/libnet80211.a(ieee80211_input.o): In function `ieee80211_deliver_data':
/home/wchen/gwen/nonos/app/net80211/ieee80211_input.c:543: undefined reference to `pbuf_alloc'
../lib/weak/libnet80211.a(wl_cnx.o): In function `cnx_update_bss_more':
/home/wchen/gwen/nonos/app/net80211/wl_cnx.c:1700: undefined reference to `dhcp_release'
/home/wchen/gwen/nonos/app/net80211/wl_cnx.c:1700: undefined reference to `dhcp_stop'
/home/wchen/gwen/nonos/app/net80211/wl_cnx.c:1700: undefined reference to `dhcp_cleanup'
../lib/weak/libnet80211.a(wl_cnx.o): In function `cnx_sta_leave':
/home/wchen/gwen/nonos/app/net80211/wl_cnx.c:1700: undefined reference to `netif_set_down'
/home/wchen/gwen/nonos/app/net80211/wl_cnx.c:1700: undefined reference to `dhcp_release'
/home/wchen/gwen/nonos/app/net80211/wl_cnx.c:1700: undefined reference to `dhcp_stop'
/home/wchen/gwen/nonos/app/net80211/wl_cnx.c:1700: undefined reference to `dhcp_cleanup'
../lib/weak/libmain.a(eagle_lwip_if.o): In function `lwip_if0_task':
/home/wchen/gwen/nonos/app/main/eagle_lwip_if.c:59: undefined reference to `pbuf_free'
../lib/weak/libmain.a(eagle_lwip_if.o): In function `lwip_if1_task':
/home/wchen/gwen/nonos/app/main/eagle_lwip_if.c:78: undefined reference to `pbuf_free'
../lib/weak/libmain.a(eagle_lwip_if.o): In function `eagle_low_level_init':
/home/wchen/gwen/nonos/app/main/eagle_lwip_if.c:121: undefined reference to `etharp_output'
/home/wchen/gwen/nonos/app/main/eagle_lwip_if.c:121: undefined reference to `ethernet_input'
/home/wchen/gwen/nonos/app/main/eagle_lwip_if.c:121: undefined reference to `netif_add'
/home/wchen/gwen/nonos/app/main/eagle_lwip_if.c:121: undefined reference to `dhcps_start'
../lib/weak/libmain.a(eagle_lwip_if.o): In function `eagle_lwip_if_alloc':
/home/wchen/gwen/nonos/app/main/eagle_lwip_if.c:206: undefined reference to `netif_set_addr'
/home/wchen/gwen/nonos/app/main/eagle_lwip_if.c:233: undefined reference to `netif_add'
/home/wchen/gwen/nonos/app/main/eagle_lwip_if.c:251: undefined reference to `dhcps_start'
/home/wchen/gwen/nonos/app/main/eagle_lwip_if.c:274: undefined reference to `netif_add'
/home/wchen/gwen/nonos/app/main/eagle_lwip_if.c:274: undefined reference to `netif_remove'
/home/wchen/gwen/nonos/app/main/eagle_lwip_if.c:274: undefined reference to `dhcps_stop'
/home/wchen/gwen/nonos/app/main/eagle_lwip_if.c:238: undefined reference to `netif_remove'
/home/wchen/gwen/nonos/app/main/eagle_lwip_if.c:244: undefined reference to `dhcps_stop'
/home/wchen/gwen/nonos/app/main/eagle_lwip_if.c:246: undefined reference to `netif_remove'
../lib/weak/libmain.a(eagle_lwip_if.o):(.text.low_level_output+0x4): undefined reference to `pbuf_alloc'
../lib/weak/libmain.a(eagle_lwip_if.o):(.text.low_level_output+0x8): undefined reference to `pbuf_copy'
../lib/weak/libmain.a(eagle_lwip_if.o):(.text.low_level_output+0xc): undefined reference to `pbuf_free'
../lib/weak/libmain.a(eagle_lwip_if.o): In function `low_level_output':
/home/wchen/gwen/nonos/app/main/eagle_lwip_if.c:154: undefined reference to `pbuf_alloc'
/home/wchen/gwen/nonos/app/main/eagle_lwip_if.c:154: undefined reference to `pbuf_copy'
/home/wchen/gwen/nonos/app/main/eagle_lwip_if.c:157: undefined reference to `pbuf_free'
../lib/weak/libmain.a(user_interface.o): In function `system_station_got_ip_set':
/home/wchen/gwen/nonos/app/main/user_interface.c:1487: undefined reference to `dhcps_start'
/home/wchen/gwen/nonos/app/main/user_interface.c:1495: undefined reference to `dhcps_stop'
/home/wchen/gwen/nonos/app/main/user_interface.c:1498: undefined reference to `dhcp_start'
../lib/weak/libmain.a(user_interface.o): In function `system_print_meminfo':
/home/wchen/gwen/nonos/app/main/user_interface.c:1512: undefined reference to `dhcp_stop'
/home/wchen/gwen/nonos/app/main/user_interface.c:1515: undefined reference to `dhcp_stop'
../lib/weak/libmain.a(user_interface.o): In function `system_get_chip_id':
/home/wchen/gwen/nonos/app/main/user_interface.c:1524: undefined reference to `dhcp_cleanup'
../lib/weak/libmain.a(user_interface.o): In function `system_init_done_cb':
/home/wchen/gwen/nonos/app/main/user_interface.c:1573: undefined reference to `netif_set_default'
../lib/weak/libmain.a(user_interface.o): In function `system_get_data_of_array_8':
/home/wchen/gwen/nonos/app/main/user_interface.c:1584: undefined reference to `netif_default'
../lib/weak/libmain.a(user_interface.o): In function `system_get_string_from_flash':
/home/wchen/gwen/nonos/app/main/user_interface.c:1613: undefined reference to `netif_set_default'
/home/wchen/gwen/nonos/app/main/user_interface.c:1624: undefined reference to `netif_set_default'
../lib/weak/libmain.a(user_interface.o): In function `wifi_softap_get_config_default':
/home/wchen/gwen/nonos/app/main/user_interface.c:3122: undefined reference to `netif_set_default'
../lib/weak/libmain.a(user_interface.o): In function `wifi_softap_get_station_info':
/home/wchen/gwen/nonos/app/main/user_interface.c:3354: undefined reference to `netif_set_addr'
../lib/weak/libmain.a(app_main.o): In function `flash_data_check':
/home/wchen/gwen/nonos/app/main/app_main.c:1025: undefined reference to `lwip_init'
/home/wchen/gwen/nonos/app/main/app_main.c:1022: undefined reference to `espconn_init'
../lib/weak/libmain.a(app_main.o): In function `user_local_init':
/home/wchen/gwen/nonos/app/main/app_main.c:1130: undefined reference to `netif_set_default'
../lib/weak/libnet80211.a(ieee80211_output.o):(.text.ieee80211_output_pbuf+0x1c): undefined reference to `pbuf_ref'
../lib/weak/libnet80211.a(ieee80211_output.o): In function `ieee80211_output_pbuf':
/home/wchen/gwen/nonos/app/net80211/ieee80211_output.c:210: undefined reference to `pbuf_free'
../lib/weak/libnet80211.a(ieee80211_output.o): In function `ieee80211_output_pbuf':
/home/wchen/gwen/nonos/app/net80211/../../include/lldesc/lldesc.h:127: undefined reference to `pbuf_ref'
../third_party/relib/.output/eagle/debug/lib/librelib.a(user_start.o): In function `relib_read_macaddr_from_otp':
user_start.c:(.irom0.text+0xec): undefined reference to `sys_check_timeouts'
user_start.c:(.irom0.text+0x15c): undefined reference to `lwip_init'
user_start.c:(.irom0.text+0x160): undefined reference to `espconn_init'
../third_party/relib/.output/eagle/debug/lib/librelib.a(user_start.o): In function `relib_user_local_init':
/ssdhome/ranma/src/esp8266-nonos-sdk-re-ranma-git/third_party/relib/main/user_start.c:434: undefined reference to `lwip_init'
/ssdhome/ranma/src/esp8266-nonos-sdk-re-ranma-git/third_party/relib/main/user_start.c:435: undefined reference to `espconn_init'
/ssdhome/ranma/src/esp8266-nonos-sdk-re-ranma-git/third_party/relib/main/user_start.c:645: undefined reference to `netif_set_default'
user/.output/eagle/debug/lib/libuser.a(user_main.o): In function `print_sta_config':
/ssdhome/ranma/src/esp8266-nonos-sdk-re-ranma-git/hello_nolwip/user/user_main.c:58: undefined reference to `wifi_softap_set_dhcps_offer_option'
/ssdhome/ranma/src/esp8266-nonos-sdk-re-ranma-git/hello_nolwip/user/user_main.c:61: undefined reference to `wifi_softap_set_dhcps_offer_option'
../lib/weak/libnet80211.a(ieee80211.o):/home/wchen/gwen/nonos/app/net80211/ieee80211.c:355: undefined reference to `ethbroadcast'
../lib/weak/libnet80211.a(ieee80211_power.o): In function `ieee80211_pwrsave':
/home/wchen/gwen/nonos/app/net80211/ieee80211_power.c:330: undefined reference to `pbuf_free'
../lib/weak/libnet80211.a(ieee80211_power.o): In function `pwrsave_flushq':
/home/wchen/gwen/nonos/app/net80211/ieee80211_power.c:452: undefined reference to `pbuf_free'
/home/wchen/gwen/nonos/app/net80211/ieee80211_power.c:460: undefined reference to `pbuf_free'
../lib/weak/libwpa.a(wpa_auth.o): In function `__wpa_send_eapol':
/home/wchen/gwen/nonos/app/wpa/wpa_auth.c:1813: undefined reference to `pbuf_alloc'
../lib/weak/libwpa.a(wpa_main.o):(.text.eagle_auth_done+0x1c): undefined reference to `dhcp_start'
../lib/weak/libwpa.a(wpa_main.o):(.text.eagle_auth_done+0x20): undefined reference to `netif_set_addr'
../lib/weak/libwpa.a(wpa_main.o): In function `eagle_auth_done':
/home/wchen/gwen/nonos/app/wpa/wpa_main.c:305: undefined reference to `dhcp_start'
/home/wchen/gwen/nonos/app/wpa/wpa_main.c:309: undefined reference to `netif_set_addr'
/home/wchen/gwen/nonos/app/wpa/wpa_main.c:309: undefined reference to `netif_set_up'
../lib/weak/libwpa.a(wpas_glue.o): In function `wpa_sm_alloc_eapol':
/home/wchen/gwen/nonos/app/wpa/wpas_glue.c:62: undefined reference to `pbuf_alloc'
collect2: error: ld returned 1 exit status
make: *** [../Makefile:400: .output/eagle/debug/image/eagle.app.v6.out] Error 1
*/

#define LWIP_DNS 1

#include "lwip/netif.h"
#include "lwip/inet.h"
#include "netif/etharp.h"
#include "lwip/udp.h"
#include "lwip/ip.h"
#include "lwip/init.h"
#include "lwip/memp.h"
#include "lwip/dns.h"

#include "ets_sys.h"
#include "os_type.h"

#include "user_config.h"

#define MAX_LEN 512

/* Can't include user_interface.h here since it is incompatible with lwip * headers */
#define SOFTAP_IF 0x01
bool wifi_get_ip_info(uint8_t if_index, ip_addr_t *info);

static const char mem_debug_file[] ICACHE_RODATA_ATTR = __FILE__;

typedef struct __attribute__((packed)) {
	uint16_t id;
	union {
		uint8_t flags1;
		struct {
			uint8_t recurse:1;
			uint8_t trunc:1;
			uint8_t authoritative:1;
			uint8_t op:4;
			uint8_t resp:1;
		} __attribute__((packed));
	};
	union {
		uint8_t flags2;
		struct {
			uint8_t err:4;
			uint8_t resvd:1;
			uint8_t authenticated:1;
			uint8_t accept_unauth:1;
			uint8_t recursion_available:1;
		} __attribute__((packed));
	};
	uint16_t n_qd;
	uint16_t n_an;
	uint16_t n_ns;
	uint16_t n_ar;
} dns_hdr_st;

typedef struct __attribute__((packed)) {
	uint16_t type;
	uint16_t cls;
} dns_query_st;

typedef struct __attribute__((packed)) {
	dns_query_st q;
	uint32_t ttl;
	uint16_t len;
} dns_answer_st;

typedef struct __attribute__((packed)) {
	uint32_t serial;
	uint32_t refresh;
	uint32_t retry;
	uint32_t expire;
	uint32_t min_ttl;
} dns_soa_st;

typedef union __attribute__((packed)) {
	uint16_t val;
	struct {
		uint8_t l;
		uint8_t h;
	};
} dns_ptr_u;


static void ICACHE_FLASH_ATTR
handle_dns(void *arg, struct udp_pcb *pcb, struct pbuf *p, struct ip_addr *addr, uint16_t port)
{
	struct pbuf *out = NULL;
	static dns_hdr_st hdr;
	int i;

	if (p->tot_len < sizeof(dns_hdr_st)) {
		os_printf("ignoring short DNS packet\n");
		goto exit_free;
	}
	pbuf_copy_partial(p, &hdr, sizeof(hdr), 0);

	/* pbuf_alloc with PBUF_RAM allocates a contiguous buffer */
	out = pbuf_alloc(PBUF_TRANSPORT, MAX_LEN, PBUF_RAM);
	if (out == NULL) {
		os_printf("could not allocate memory for answer\n");
		goto exit_free;
	}

	if (hdr.n_an != 0 || hdr.n_ns != 0 | hdr.n_ar != 0) {
		os_printf("ignoring unexpected DNS packet\n");
		goto exit_free;
	}

	pbuf_copy_partial(p, out->payload, p->tot_len, 0);
	dns_hdr_st *out_hdr = out->payload;
	out_hdr->resp = 1;

	int n_queries = lwip_ntohs(hdr.n_qd);
	uint8_t *buf = &((uint8_t*)out->payload)[p->tot_len];
	int p_ofs = sizeof(hdr);
	int n_an = 0;
	for (i = 0; i < n_queries && p_ofs < p->tot_len; i++) {
		uint8_t *rewind = buf;
		dns_ptr_u name_ofs = {.val = (p_ofs | 0xc000)};

		*buf++ = name_ofs.h;
		*buf++ = name_ofs.l;

		/* Skip over name.
		 * Note: pbuf_get_at returns 0 if p_ofs is out of range,
		 * so this will always terminate. */
		while (pbuf_get_at(p, p_ofs) != 0) p_ofs++;
		p_ofs++;

		dns_query_st query;
		if (pbuf_copy_partial(p, &query, sizeof(query), p_ofs) != sizeof(query)) {
			break;
		}
		p_ofs += sizeof(query);
		uint16_t q_type = PP_HTONS(query.type);
		uint16_t q_cls = PP_HTONS(query.cls);

		dns_answer_st *answer = (void*)buf;
		answer->q = query;
		answer->ttl = PP_HTONS(0); /* do not cache */
		buf += sizeof(*answer);

		if (q_cls != DNS_RRCLASS_IN) {
			/* Can't handle this query, undo the name copy */
			buf = rewind;
			continue;
		}

		switch (q_type) {
		case DNS_RRTYPE_A: {
			answer->len = PP_HTONS(4);
			ip_addr_t ip = {0};
			wifi_get_ip_info(SOFTAP_IF, &ip);
			buf[0] = ip4_addr1(&ip);
			buf[1] = ip4_addr2(&ip);
			buf[2] = ip4_addr3(&ip);
			buf[3] = ip4_addr4(&ip);
			buf += sizeof(ip);

			break; }
		case DNS_RRTYPE_NS: {
			answer->len = PP_HTONS(4);
			*buf++ = 2;
			*buf++ = 'n';
			*buf++ = 's';
			*buf++ = 0;
			break; }
		case DNS_RRTYPE_SOA: {
			dns_soa_st *soa = (void*)&buf[4];
			answer->len = PP_HTONS(4 + sizeof(*soa));
			/* Primary nameserver */
			*buf++ = name_ofs.h;
			*buf++ = name_ofs.l;
			/* Responsible mailbox */
			*buf++ = name_ofs.h;
			*buf++ = name_ofs.l;
			soa->serial = PP_HTONL(12345);
			soa->refresh = PP_HTONL(300);   /* 5 min */
			soa->retry = PP_HTONL(150);     /* 2.5 min */
			soa->expire = PP_HTONL(3600);   /* 1 hour */
			soa->min_ttl = PP_HTONL(60);    /* 1 min */
			buf += sizeof(*soa);
			break; }
		default:
			/* Can't handle this query, undo the name copy */
			buf = rewind;
			continue;
		}
		n_an++;
	}
	/* Update answer count */
	out_hdr->n_an = PP_HTONS(n_an);
	/* Resize pbuf to the actual answer length */
	pbuf_realloc(out, buf - (uint8_t*)out->payload);

	udp_sendto(pcb, out, addr, port);

exit_free:
	if (out != NULL) {
		pbuf_free(out);
	}
	pbuf_free(p);
}

static struct udp_pcb *pcb;

int ICACHE_FLASH_ATTR
cdnsd_init(uint32_t ip_addr, int port)
{
	ip_addr_t lwip_addr = {ip_addr};

	pcb = udp_new();
	if (pcb == NULL) {
		return -1;
	}

	err_t err = udp_bind(pcb, &lwip_addr, port);
	if (err != ERR_OK) {
		memp_free(MEMP_UDP_PCB, pcb);
		return -2;
	}

	udp_recv(pcb, handle_dns, NULL);
	return 0;
}

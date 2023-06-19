#define MEM_DEFAULT_USE_DRAM 1

#include <assert.h>
#include <stdint.h>
#include "c_types.h"
#include "mem.h"

#include "relib/s/lldesc.h"
#include "relib/s/esf_buf.h"
#include "relib/s/esf_buf_ctx.h"
#include "relib/s/esf_mgmt_rawsbuf.h"
#include "relib/s/esf_mgmt_rawlbuf.h"
#include "relib/s/esf_rx_desc.h"
#include "relib/s/esf_tx_desc.h"
#include "relib/s/wdev_control.h"
#include "relib/xtensa.h"

#include "lwip/pbuf.h"

#if LIBLWIP_VER == 2
#define PB_TYPE(pb) (pb->type_internal)
#else
#define PB_TYPE(pb) (pb->type)
#endif

#define DEBUG 0

/*
3ffee9e0 V ebCtx_30
3ffee9f4 V g_free_rxblock_eb_cnt_32
3ffeea00 V eb_space_43
3ffeee40 V eb_mgmt_lbuf_space_44
3ffeef50 V eb_mgmt_sbuf_space_45
3ffef1b0 V eb_txdesc_space_46
3ffef430 V eb_rxdesc_space_47
3ffef490 V interface_mask
*/

#define EP_OFFSET 36

static const char mem_debug_file[] ICACHE_RODATA_ATTR STORE_ATTR = "esf_buf.c";

extern wdev_control_st wDevCtrl;

esf_buf_ctx_st ebCtx;

/* esf_buf_st is 40 byte */
esf_buf_st eb_space[27];                    /* 1088 / 40 => 27 */
/* esf_mgmt_rawlbuf is 268 byte */
esf_mgmt_rawlbuf_st eb_mgmt_lbuf_space[1];  /* 272 / 268 =>  1 */
/* esf_mgmt_rawsbuf is 76 byte */
esf_mgmt_rawsbuf_st eb_mgmt_sbuf_space[8];  /* 608 / 76  =>  8 */
/* esf_tx_desc_st is 32 byte */
esf_tx_desc_st eb_txdesc_space[20];         /* 640 / 32  => 20 */
/* esf_rx_desc_st is 12 byte */
esf_rx_desc_st eb_rxdesc_space[8];          /* 96 / 12   =>  8 */

static inline bool ICACHE_FLASH_ATTR
pbuf_is_ram_type(struct pbuf *p)
{
#if LIBLWIP_VER == 2
	return pbuf_get_allocsrc(p) == PBUF_TYPE_ALLOC_SRC_MASK_STD_HEAP;
#else
	return p->type == PBUF_RAM;
#endif
}

#define GET_FREELIST_ENTRY(freelist) ({ \
	uint32_t saved = LOCK_IRQ_SAVE(); \
	esf_buf_st *entry = *freelist; \
	if (entry != NULL) { \
		*freelist = entry->stqe_next; \
		entry->stqe_next = NULL; \
	} \
	LOCK_IRQ_RESTORE(saved); \
	entry; \
})

esf_buf_st* ICACHE_FLASH_ATTR
esf_get_freelist_entry(esf_buf_st **freelist)
{
	return GET_FREELIST_ENTRY(freelist);
}

void ICACHE_FLASH_ATTR
esf_put_freelist_entry(esf_buf_st **freelist, esf_buf_st *eb)
{
	uint32_t saved = LOCK_IRQ_SAVE();
	eb->stqe_next = *freelist;
	*freelist = eb;
	LOCK_IRQ_RESTORE(saved);
}

struct buf_holder {
	esf_buf_st eb;
	lldesc_st head;
	esf_tx_desc_st tx_desc;
	uint8_t buf[0];
};

esf_buf_st* ICACHE_FLASH_ATTR
esf_buf_alloc(struct pbuf *pb, esf_buf_type_t type, uint32_t len)
{
#if DEBUG == 1
	void *caller = __builtin_return_address(0);
	os_printf("[@%p] esf_buf_alloc(%p, %d, %d)\n",
		caller, pb, type, len);
#endif
	if ((type == ESF_BUF_TX_PB) && (pb != NULL)) {
		if ((uint32_t)pb >= 0x40000000) {
			os_printf("s_pb:0x%08x\n", (uint32_t)pb);
			return NULL;
		}
		esf_buf_st *entry = esf_get_freelist_entry(&ebCtx.eb_tx_free_list);
		if (entry == NULL) {
			return NULL;
		}

		memset(entry->tx_desc,0,sizeof(*entry->tx_desc));
		entry->tx_desc->flags = 0x2000;

		entry->pbuf = pb;
		/* Not sure why this is conditional */
		if (pbuf_is_ram_type(pb)) {
			pb->eb = entry;
		} else if (pb->eb != NULL) {
			/* lwip1_pbuf_alloc is setting pb->eb to NULL */
			void *caller = __builtin_return_address(0);
			/* Seeing some cases where pb->eb is an invalid
			 * pointer when lwip1_pbuf_free is called */
			os_printf("[@%p] esf_buf_alloc unexpected pb->eb=%p != NULL, pb->type=%d\n",
				caller, pb->eb, PB_TYPE(pb));
			/* looks like this is happening in ieee80211_output_pbuf with type=PBUF_REF
			 * and only hostap_deliver_data and ieee80211_deliver_data are
			 * making a PBUF_REF alloc and setting pb->eb directly.
			 */
		}

		uint8_t *buf = pb->payload;
		entry->ds_head->buf = buf;
		entry->data_len = pb->len;
		entry->buf_begin = buf - EP_OFFSET;
		return entry;
	}
	else if (type == ESF_BUF_MGMT_SBUF) {
		esf_buf_st *entry = esf_get_freelist_entry(&ebCtx.eb_mgmt_s_free_list);
		if (entry == NULL) {
			return NULL;
		}
		memset(entry->tx_desc,0,sizeof(*entry->tx_desc));
		entry->tx_desc->flags = 0x8000;
		entry->ds_head->buf = entry->buf_begin;
		return entry;
	}
	else if (type == ESF_BUF_BAR) {
		esf_buf_st *entry = esf_get_freelist_entry(&ebCtx.eb_tx_bar_free_list);
		if (entry == NULL) {
			return NULL;
		}
		entry->tx_desc->flags = 0x200000;
		return entry;
	}
	else if (type == ESF_BUF_MGMT_LBUF || type == ESF_BUF_MGMT_LLBUF) {
		struct buf_holder *holder = os_zalloc(sizeof(*holder) + len);
		if (holder == NULL) {
			return NULL;
		}
		if ((uint32_t)holder >= 0x40000000) {
			os_free(holder);
			return NULL;
		}
		esf_buf_st *eb = &holder->eb;
		eb->ds_head = &holder->head;
		eb->ds_tail = &holder->head;
		eb->ds_len = 1;
		eb->tx_desc = &holder->tx_desc;
		if (type == ESF_BUF_MGMT_LBUF) {
			eb->tx_desc->flags = 0x4000;
		} else {
			eb->tx_desc->flags = 0x1000000;
		}
		eb->buf_begin = holder->buf;
		eb->ds_head->buf = holder->buf;
		return eb;
	}
	return NULL;
}

void ICACHE_FLASH_ATTR
esf_buf_recycle(esf_buf_st *eb, esf_buf_type_t type)
{
#if DEBUG == 1
	void *caller = __builtin_return_address(0);
	os_printf("[@%p] esf_buf_recycle(%p, %d)\n",
		caller, eb, type);
#endif
	if ((type == ESF_BUF_TX_PB) || (type == ESF_BUF_TX_SIP)) {
		memset(eb->tx_desc,0,0x20);
		esf_put_freelist_entry(&ebCtx.eb_tx_free_list, eb);
	}
	else if (type == ESF_BUF_MGMT_SBUF) {
		memset(eb->tx_desc,0,0x20);
		esf_put_freelist_entry(&ebCtx.eb_mgmt_s_free_list, eb);
	}
	else if (type == ESF_BUF_BAR) {
		memset(eb->tx_desc,0,0x20);
		esf_put_freelist_entry(&ebCtx.eb_tx_bar_free_list, eb);
	}
	else if (type == ESF_BUF_RX_BLOCK) {
		memset(eb->rx_desc,0,0xc);
		esf_put_freelist_entry(&ebCtx.eb_rx_block_free_list, eb);
	}
	else if (type == ESF_BUF_MGMT_LBUF || type == ESF_BUF_MGMT_LLBUF) {
		os_free(eb);
	}
}

void ICACHE_FLASH_ATTR
esf_buf_setup(void)
{
	esf_buf_st *eb;
	esf_tx_desc_st *txDesc;
	esf_rx_desc_st *rxDesc;
	int i;
	esf_mgmt_rawsbuf_st *rawsbuf;

	i = 0x60;

	txDesc = eb_txdesc_space;

	eb = eb_space;
	do {
		eb->ds_len = 1;
		eb->tx_desc = txDesc;
		uint8_t *lldp = (uint8_t*)(wDevCtrl.tx_head-1);
		eb->buf_begin = (wDevCtrl.tx_head)->buf;
		eb->ds_head = (lldesc_st *)(lldp + i);
		eb->ds_tail = (lldesc_st *)(lldp + i);
		esf_buf_recycle(eb,ESF_BUF_TX_PB);
		eb = eb + 1;
		txDesc = txDesc + 1;
		i = i + -0xc;
	} while (0 < i);

	i = 8;
	rawsbuf = eb_mgmt_sbuf_space;
	do {
		rawsbuf->link.size = 0x40;
		eb->ds_head = &rawsbuf->link;
		eb->ds_tail = &rawsbuf->link;
		eb->tx_desc = txDesc;
		eb->ds_len = 1;
		eb->buf_begin = rawsbuf->buf;
		esf_buf_recycle(eb,ESF_BUF_MGMT_SBUF);
		eb = eb + 1;
		rawsbuf++;
		txDesc = txDesc + 1;
		i = i + -1;
	} while (0 < i);

	i = 4;
	do {
		eb->tx_desc = txDesc;
		esf_buf_recycle(eb,ESF_BUF_BAR);
		eb = eb + 1;
		txDesc++;
		i = i + -1;
	} while (0 < i);

	i = 7;
	rxDesc = eb_rxdesc_space;
	do {
		(eb->desc).tx_desc = (esf_tx_desc_st *)rxDesc;
		esf_buf_recycle(eb,ESF_BUF_RX_BLOCK);
		eb = eb + 1;
		rxDesc++;
		i = i + -1;
	} while (0 < i);
}

esf_buf_st* /* IRAM */
esf_rx_buf_alloc(esf_buf_type_t type)
{
	if (type == ESF_BUF_RX_BLOCK) {
		return GET_FREELIST_ENTRY(&ebCtx.eb_rx_block_free_list);
	}
	return NULL;
}

/*
         U ets_intr_lock
         U ets_intr_unlock
         U ets_memset
         U ets_printf
         U pvPortMalloc
         U pvPortZalloc
         U vPortFree
         U wDevCtrl
00000000 b ebCtx_30
00000000 t mem_debug_file_37
00000014 b g_free_rxblock_eb_cnt_32
00000018 T esf_rx_buf_alloc
00000020 b eb_space_43
000000d8 T esf_buf_alloc
00000460 b eb_mgmt_lbuf_space_44
000004c0 T esf_buf_recycle
00000570 b eb_mgmt_sbuf_space_45
00000608 T esf_buf_setup
000007d0 b eb_txdesc_space_46
00000a50 b eb_rxdesc_space_47
*/

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

static const char mem_debug_file[] ICACHE_RODATA_ATTR STORE_ATTR = "esf_buf.c";

extern wdev_control_st wDevCtrl;

esf_buf_ctx_st ebCtx;
int g_free_rxblock_eb_cnt;

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

esf_buf_st* ICACHE_FLASH_ATTR
esf_buf_alloc(struct pbuf *pb, esf_buf_type_t type, uint32_t len)
{
	uint16_t uVar1;
	uint32_t uVar2;
	esf_buf_st *new_eb;
	esf_buf_st *freelist_head;
	esf_buf_st *stqe;
	lldesc_st *plVar3;
	uint8_t *puVar4;
	uint32_t uVar5;
	esf_tx_desc_st *peVar6;
	uint8_t *puVar7;
	
	if ((type == ESF_BUF_TX_PB) && (pb != NULL)) {
		if ((uint32_t)pb >= 0x40000000) {
			os_printf("s_pb:0x%08x\n", (uint32_t)pb);
			return NULL;
		}
		uint32_t saved = LOCK_IRQ_SAVE();
		freelist_head = ebCtx.eb_tx_free_list;
		if (ebCtx.eb_tx_free_list == NULL) {
			LOCK_IRQ_RESTORE(saved);
			return NULL;
		}
		stqe = ((ebCtx.eb_tx_free_list)->bqentry).stqe_next;
		((ebCtx.eb_tx_free_list)->bqentry).stqe_next = NULL;
		ebCtx.eb_tx_free_list = stqe;
		LOCK_IRQ_RESTORE(saved);
		memset((freelist_head->desc).tx_desc,0,0x20);
		freelist_head->pbuf = pb;
		if (pbuf_is_ram_type(pb)) {
			pb->eb = freelist_head;
		}
		uVar2 = 0x2000;
		uVar1 = pb->len;
		peVar6 = (freelist_head->desc).tx_desc;
		puVar7 = (u8 *)pb->payload;
		uVar5 = *(uint32_t *)peVar6;
		freelist_head->ds_head->buf = puVar7;
		freelist_head->data_len = uVar1;
		freelist_head->buf_begin = puVar7 + -0x24;
		*(uint32_t *)peVar6 = uVar5 & 0x3f | (uVar5 >> 6 | uVar2) << 6;
		return freelist_head;
	}
	else if (type == ESF_BUF_MGMT_LBUF) {
		new_eb = (esf_buf_st *)pvPortMalloc(0x28,mem_debug_file,0x170,false);
		if (new_eb == NULL) {
			return NULL;
		}
		if ((uint32_t)new_eb < 0x40000000) {
			plVar3 = (lldesc_st *)pvPortMalloc(0xc,mem_debug_file,0x179,false);
			new_eb->ds_head = plVar3;
			if (plVar3 != NULL) {
				if (plVar3 < (lldesc_st *)0x40000001) {
					new_eb->ds_tail = plVar3;
					new_eb->ds_len = 1;
					peVar6 = (esf_tx_desc_st *)pvPortZalloc(0x20,mem_debug_file,0x184);
					(new_eb->desc).tx_desc = peVar6;
					if (peVar6 != NULL) {
						if (peVar6 < (esf_tx_desc_st *)0x40000001) {
							memset(peVar6,0,0x20);
							uVar2 = *(uint32_t *)(new_eb->desc).tx_desc;
							*(uint32_t *)(new_eb->desc).tx_desc = uVar2 & 0x3f | (uVar2 >> 6 | 0x4000) << 6;
							puVar4 = (uint8_t *)pvPortMalloc(len,mem_debug_file,0x191,false);
							new_eb->buf_begin = puVar4;
							if (puVar4 != NULL) {
								if (puVar4 < (uint8_t *)0x40000001) {
									new_eb->ds_head->buf = puVar4;
									return new_eb;
								}
								vPortFree(puVar4,mem_debug_file,0x194);
							}
							vPortFree((new_eb->desc).tx_desc,mem_debug_file,0x196);
							(new_eb->desc).tx_desc = NULL;
							vPortFree(new_eb->ds_head,mem_debug_file,0x198);
							new_eb->ds_head = NULL;
							vPortFree(new_eb,mem_debug_file,0x19a);
							return NULL;
						}
						vPortFree(peVar6,mem_debug_file,0x187);
					}
					vPortFree(new_eb->ds_head,mem_debug_file,0x189);
					new_eb->ds_head = NULL;
					vPortFree(new_eb,mem_debug_file,0x18b);
					return NULL;
				}
				vPortFree(plVar3,mem_debug_file,0x17c);
			}
			vPortFree(new_eb,mem_debug_file,0x17e);
		}
		else {
			vPortFree(new_eb,mem_debug_file,0x173);
		}
	}
	else if (type == ESF_BUF_MGMT_SBUF) {
		uint32_t saved = LOCK_IRQ_SAVE();
		freelist_head = ebCtx.eb_mgmt_s_free_list;
		if (ebCtx.eb_mgmt_s_free_list == NULL) {
			LOCK_IRQ_RESTORE(saved);
			return NULL;
		}
		stqe = ((ebCtx.eb_mgmt_s_free_list)->bqentry).stqe_next;
		((ebCtx.eb_mgmt_s_free_list)->bqentry).stqe_next = NULL;
		ebCtx.eb_mgmt_s_free_list = stqe;
		LOCK_IRQ_RESTORE(saved);
		memset((freelist_head->desc).tx_desc,0,0x20);
		uVar2 = 0x8000;
		peVar6 = (freelist_head->desc).tx_desc;
		freelist_head->ds_head->buf = freelist_head->buf_begin;
		uVar5 = *(uint32_t *)peVar6;
		*(uint32_t *)peVar6 = uVar5 & 0x3f | (uVar5 >> 6 | uVar2) << 6;
		return freelist_head;
	}
	else if (type == ESF_BUF_MGMT_LLBUF) {
		freelist_head = (esf_buf_st *)pvPortMalloc(0x28,mem_debug_file,0x1bd,false);
		if (freelist_head < (esf_buf_st *)0x40000001) {
			if (freelist_head == NULL) {
				return NULL;
			}
			plVar3 = (lldesc_st *)pvPortMalloc(0xc,mem_debug_file,0x1c5,false);
			freelist_head->ds_head = plVar3;
			if (plVar3 != NULL) {
				if (plVar3 < (lldesc_st *)0x40000001) {
					freelist_head->ds_tail = plVar3;
					freelist_head->ds_len = 1;
					peVar6 = (esf_tx_desc_st *)pvPortMalloc(0x20,mem_debug_file,0x1d0,false);
					(freelist_head->desc).tx_desc = peVar6;
					if (peVar6 != NULL) {
						if (peVar6 < (esf_tx_desc_st *)0x40000001) {
							memset(peVar6,0,0x20);
							uVar2 = *(uint32_t *)(freelist_head->desc).tx_desc;
							*(uint32_t *)(freelist_head->desc).tx_desc = uVar2 & 0x3f | (uVar2 >> 6 | 0x1000000) << 6;
							puVar4 = (uint8_t *)pvPortMalloc(len,mem_debug_file,0x1dd,false);
							freelist_head->buf_begin = puVar4;
							if (puVar4 != NULL) {
								if (puVar4 < (uint8_t *)0x40000001) {
									freelist_head->ds_head->buf = puVar4;
									return freelist_head;
								}
								vPortFree(puVar4,mem_debug_file,0x1e0);
							}
							vPortFree((freelist_head->desc).tx_desc,mem_debug_file,0x1e2);
							(freelist_head->desc).tx_desc = NULL;
							vPortFree(freelist_head->ds_head,mem_debug_file,0x1e4);
							freelist_head->ds_head = NULL;
							vPortFree(freelist_head,mem_debug_file,0x1e6);
							return NULL;
						}
						vPortFree(peVar6,mem_debug_file,0x1d3);
					}
					vPortFree(freelist_head->ds_head,mem_debug_file,0x1d5);
					freelist_head->ds_head = NULL;
					vPortFree(freelist_head,mem_debug_file,0x1d7);
					return NULL;
				}
				vPortFree(plVar3,mem_debug_file,0x1c8);
			}
			vPortFree(freelist_head,mem_debug_file,0x1ca);
		}
		else {
			vPortFree(freelist_head,mem_debug_file,0x1c0);
		}
	}
	else if (type == ESF_BUF_BAR) {
		uint32_t saved = LOCK_IRQ_SAVE();
		freelist_head = ebCtx.eb_tx_bar_free_list;
		if (ebCtx.eb_tx_bar_free_list != NULL) {
			stqe = ((ebCtx.eb_tx_bar_free_list)->bqentry).stqe_next;
			((ebCtx.eb_tx_bar_free_list)->bqentry).stqe_next = NULL;
			ebCtx.eb_tx_bar_free_list = stqe;
			LOCK_IRQ_RESTORE(saved);
			peVar6 = (freelist_head->desc).tx_desc;
			uVar2 = 0x200000;
			uVar5 = *(uint32_t *)peVar6;
			*(uint32_t *)peVar6 = uVar5 & 0x3f | (uVar5 >> 6 | uVar2) << 6;
			return freelist_head;
		}
		LOCK_IRQ_RESTORE(saved);
	}
	return NULL;
}

void ICACHE_FLASH_ATTR
esf_buf_recycle(esf_buf_st *eb, esf_buf_type_t type)
{
	if ((type == ESF_BUF_TX_PB) || (type == ESF_BUF_TX_SIP)) {
		memset(eb->tx_desc,0,0x20);
		uint32_t saved = LOCK_IRQ_SAVE();
		eb->stqe_next = ebCtx.eb_tx_free_list;
		ebCtx.eb_tx_free_list = eb;
		LOCK_IRQ_RESTORE(saved);
	}
	else if (type == ESF_BUF_MGMT_LBUF) {
		vPortFree(eb->ds_head,mem_debug_file,0x21e);
		vPortFree(eb->tx_desc,mem_debug_file,0x220);
		vPortFree(eb->buf_begin,mem_debug_file,0x221);
		vPortFree(eb,mem_debug_file,0x222);
	}
	else if (type == ESF_BUF_MGMT_SBUF) {
		memset(eb->tx_desc,0,0x20);
		uint32_t saved = LOCK_IRQ_SAVE();
		eb->stqe_next = ebCtx.eb_mgmt_s_free_list;
		ebCtx.eb_mgmt_s_free_list = eb;
		LOCK_IRQ_RESTORE(saved);
	}
	else if (type == ESF_BUF_MGMT_LLBUF) {
		vPortFree(eb->ds_head,mem_debug_file,0x233);
		vPortFree(eb->tx_desc,mem_debug_file,0x235);
		vPortFree(eb->buf_begin,mem_debug_file,0x236);
		vPortFree(eb,mem_debug_file,0x237);
	}
	else if (type == ESF_BUF_BAR) {
		memset(eb->tx_desc,0,0x20);
		uint32_t saved = LOCK_IRQ_SAVE();
		eb->stqe_next = ebCtx.eb_tx_bar_free_list;
		ebCtx.eb_tx_bar_free_list = eb;
		LOCK_IRQ_RESTORE(saved);
	}
	else if (type == ESF_BUF_RX_BLOCK) {
		memset(eb->tx_desc,0,0xc);
		uint32_t saved = LOCK_IRQ_SAVE();
		eb->stqe_next = ebCtx.eb_rx_block_free_list;
		ebCtx.eb_rx_block_free_list = eb;
		LOCK_IRQ_RESTORE(saved);
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
	esf_buf_st *eb = NULL;

	if (type == ESF_BUF_RX_BLOCK) {
		uint32_t saved = LOCK_IRQ_SAVE();
		eb = ebCtx.eb_rx_block_free_list;
		if (eb != NULL) {
			ebCtx.eb_rx_block_free_list = eb->stqe_next;
			eb->stqe_next = NULL;
			g_free_rxblock_eb_cnt--;
		}
		LOCK_IRQ_RESTORE(saved);
	}
	return eb;
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

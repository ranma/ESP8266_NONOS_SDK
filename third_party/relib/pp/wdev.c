#include <assert.h>
#include "c_types.h"
#include "osapi.h"
#include "mem.h"

#include "netif/etharp.h"

#include "relib/esp8266.h"
#include "relib/xtensa.h"
#include "relib/ets_rom.h"
#include "relib/ets_timer.h"
#include "relib/relib.h"
#include "relib/s/lldesc.h"
#include "relib/s/esf_buf.h"
#include "relib/s/esf_rx_desc.h"
#include "relib/s/wdev_control.h"
#include "relib/s/wdev_status.h"
#include "relib/s/ieee80211com.h"
#include "relib/s/ieee80211_frame.h"
#include "relib/s/rxcontrol.h"

#if 0
#define maybe_extern extern
#else
#define maybe_extern
#endif

#define g_wDevCtrl g_wDevCtrl_83
#define my_event my_event_100
#define WdevRxIsClose WdevRxIsClose_103
#define BcnInterval BcnInterval_108
#define BcnSendTick BcnSendTick_111
#define _wdev_rx_desc_spac _wdev_rx_desc_space_115
#define _wdev_rx_mblk_space _wdev_rx_mblk_space_116
#define _wdev_tx_desc_space _wdev_tx_desc_space_118
#define _wdev_tx_mblk_space _wdev_tx_mblk_space_119
#define _wdev_rx_ampdu_len_desc_space _wdev_rx_ampdu_len_desc_space_120
#define _wdev_rx_ampdu_len_space _wdev_rx_ampdu_len_space_121

#define RX_MBLK_ENTRY_LEN 1604
#define RX_AMPDU_ENTRY_LEN 256
#define TX_MBLK_ENTRY_LEN 4

typedef void (*mac_timer_cb_t)(void);

typedef enum bcn_policy {
	RX_DISABLE=0,
	RX_ANY=1,
	RX_BSSID=2,
} bcn_policy_t;

maybe_extern wdev_control_st wDevCtrl;
//maybe_extern wdev_control_st *g_wDevCtrl = &wDevCtrl;
maybe_extern wdev_status_st my_event;
maybe_extern esf_buf_st *our_tx_eb;
maybe_extern uint64_t WdevTimOffSet;
maybe_extern bool WdevRxIsClose;
maybe_extern uint32_t BcnInterval = 102400;
maybe_extern uint32_t BcnSendTick;
maybe_extern lldesc_st _wdev_rx_desc_space[6];
maybe_extern uint8_t _wdev_rx_mblk_space[6 * RX_MBLK_ENTRY_LEN];
maybe_extern lldesc_st _wdev_tx_desc_space[8];
maybe_extern uint8_t _wdev_tx_mblk_space[8 * TX_MBLK_ENTRY_LEN];
maybe_extern mac_timer_cb_t MacTimerCb[2];
maybe_extern lldesc_st _wdev_rx_ampdu_len_desc_space[7];
maybe_extern uint8_t _wdev_rx_ampdu_len_space[7 * RX_AMPDU_ENTRY_LEN];

static_assert(sizeof(_wdev_rx_mblk_space) == 9624, "_wdev_rx_mblk_space size mismatch");
static_assert(sizeof(_wdev_tx_mblk_space) == 32, "_wdev_tx_mblk_space size mismatch");
static_assert(sizeof(_wdev_rx_ampdu_len_space) == 0x700, "_wdev_rx_ampdu_len_space size mismatch");

extern ieee80211com_st g_ic; /* ieee80211.c */
extern bool TestStaFreqCalValOK;
extern int16_t TestStaFreqCalValDev;

void PPWdtReset(void);
void wDevCleanRxBuf(lldesc_st * desc);
void wDev_Disable_Beacon_Tsf(void);
void wDev_Enable_Beacon_Tsf(void);
void wDev_MacTimerISRHdl(char idx);
void wDev_SnifferRxLDPC(void);
void wDev_SnifferRxHT40(void);
void wDev_ProcessRxSucData(lldesc_st * tail, uint16_t nblks, uint32_t rxstart_time, uint8_t rxtime_valid);
void wDev_IndicateFrame(ieee80211_channel_st * ch, bool isampdu, lldesc_st * tail, uint16_t nblks, uint32_t rxstart_time, int16_t chl_freq_offset);
void wDev_DiscardFrame(lldesc_st * tail, uint16_t nblks);
void wDev_DiscardAMPDULen(lldesc_st * tail, uint16_t nblks);
void wDev_SnifferRxData2(lldesc_st * head);
void wDev_Option_Init(void);
void wDev_Init_dummy_key(void);
void wDev_Rxbuf_Init(void);
void wDev_AutoAckRate_Init(void);
void wDev_Bssid_Init(void);
void wDev_FetchRxLink(uint16_t nblks, lldesc_st *tail);
void wDev_AppendRxBlocks(lldesc_st *head, lldesc_st *tail, uint16_t num);
void wDev_AppendRxAmpduLensBlocks(lldesc_st *head,lldesc_st *tail,uint16_t num);
void lmacProcessRtsStart(uint8_t index);
void lmacProcessTXStartData(uint8_t index);
void lmacProcessTxSuccess(uint8_t index, uint8_t ack_snr);
void lmacProcessTxRtsError(uint8_t tx_error, uint8_t index);
void lmacProcessCtsTimeout(uint8_t index);
void lmacProcessTxError(uint8_t tx_error);
void lmacProcessAckTimeout(void);
void lmacProcessTXStartData(uint8_t index);
void lmacProcessAllTxTimeout(void);
void lmacProcessCollisions(void);
void lmacProcessRtsStart(uint8_t index);
void lmacRxDone(esf_buf_st * eb);
void rcUpdateDataRxDone(rx_control_st *rxctrl);
ieee80211_channel_st *chm_get_current_channel(void);
STATUS pp_post2(uint8_t prio, ETSSignal sig, ETSParam par);
int16_t phy_get_bb_freqoffset(void);
esf_buf_st *esf_rx_buf_alloc(esf_buf_type_t type);

void
wDevCleanRxBuf(lldesc_st *desc)
{
	*(uint32_t *)desc->buf = 0xdeadbeef;
	/* Should probably be desc->size - 4 */
	*(uint32_t *)(desc->buf + desc->size) = 0xdeadbeef;
}


void
wDev_MacTim1Arm(uint32_t time_in_us)
{
	uint32_t saved = LOCK_IRQ_SAVE();

	uint32_t now_hi = WDEV_TIMER->MAC_TIM1_COUNT_HI;
	uint32_t now_lo = WDEV_TIMER->MAC_TIM1_COUNT_LO;
	uint32_t now_hi2 = WDEV_TIMER->MAC_TIM1_COUNT_HI;
	if (now_hi != now_hi2) {
		now_lo = WDEV_TIMER->MAC_TIM1_COUNT_LO;
		now_hi = now_hi2;
	}
	uint64_t alarm = (uint64_t)now_lo + time_in_us;
	uint32_t alarm_hi = now_hi + (alarm >> 32);
	WDEV_TIMER->MAC_TIM1_ALARM_LO = alarm & 0xffffffff;
	WDEV_TIMER->MAC_TIM1_ALARM_HI = alarm_hi;

	WDEV_TIMER->MAC_TIM1_CTL |= 0x80000000;

	LOCK_IRQ_RESTORE(saved);
}

void
wDev_MacTim1SetFunc(mac_timer_cb_t user_mac_timer_cb)
{
	WDEV_TIMER->MAC_TIM1_CTL2 |= 0x80000000;
	MacTimerCb[1] = user_mac_timer_cb;
}

void
wDev_MacTimerISRHdl(char idx)
{
	if (MacTimerCb[idx] != NULL) {
		MacTimerCb[idx]();
	}
}


uint8_t
wDev_SetFrameAckType(uint8_t type,uint8_t ack_type)
{
	uint32_t uVar2;
	int idx = (type >> 2 & 3) * 2 + (type >> 7);
	int shift = 0x1c - ((type >> 4 & 7) * 4);
	uint32_t mask = 0xf << shift;
	uint32_t val = ack_type << shift;

	uint32_t x = WDEV->ACK_TYPE[idx];
	WDEV->ACK_TYPE[idx] = (x & ~mask) | val;
	return (x >> shift) & 0xf;
}

void
wDevEnableRx(void)
{
	WdevRxIsClose = false;
	WDEV_BASE->RX_CTL |= 0x80000000;
}

void
wDevDisableRx(void)
{
	WDEV_BASE->RX_CTL &= 0x7fffffff;
	WdevRxIsClose = true;
}

void
wDev_EnableTransmit(uint8_t index,uint8_t aifs,uint16_t backoff)
{
	int idx = (7 - index) & 7;
	WDEV->TXCFG[idx].BACKOFF = (backoff & 0x3ff) << 12;
	WDEV->TXCFG[idx].CFG0 |= 0xc0000000;
	my_event.tx_enable_counts[index]++;
}

void
wDev_DisableTransmit(uint8_t index)
{
	int idx = (7 - index) & 7;
	WDEV->TXCFG[idx].CFG0 &= ~0xc0000000;
}

void
wDev_ClearWaitingQueue(uint8_t index)
{
	/* This is an invalid index multiplier, as it would overlap
	 * the WDEv->TXCFG registers badly if called with a low index
	 * number.
	 * However, it is only called from lmacClearWaitQueue with
	 * index == 0xa, putting the register address at 0x3ff2cdc,
	 * which is not overlapping!
	 */
	if (index != 10) {
		ets_printf("wDev_ClearWaitingQueue bad idx %d\n", index);
		return;
	}
#if 0
	volatile uint32_t *reg = (uint32_t*)(0x3ff20da4 - 10 * 0x14); /* 0x3FF20CDC */
	*reg &= 0xbfffffff;
#else
	WDEV->TX_CFG0 &= 0xbfffffff;
#endif
}

void
wDev_SetWaitingQueue(uint8_t index)
{
	uint32_t uVar1;
	int iVar2;

	if (index != 10) {
		ets_printf("wDev_SetWaitingQueue bad idx %d\n", index);
		return;
	}
	WDEV->ACK_TYPE[6] = (WDEV->ACK_TYPE[6] & 0xfffffff0) | (index & 0xf);
	WDEV->TX_CFG0 &= 0x7fffffff;
	WDEV->TX_CFG0 |= 0x40000000;
}

int wDev_Get_KeyEntry(int *alg, uint8_t *bssid_no,int *key_idx,uint8_t *addr,int key_entry_valid,uint8_t *key, int key_len)
{
	uint32_t addr_lo = WDEV_KEYENTRY->E[key_entry_valid].ADDR_LO;
	uint32_t addr_hi = WDEV_KEYENTRY->E[key_entry_valid].ADDR_HI;

	addr[0] = addr_lo;
	addr[1] = addr_lo >> 8;
	addr[2] = addr_lo >> 16;
	addr[3] = addr_lo >> 24;
	addr[4] = addr_hi;
	addr[5] = addr_hi >> 8;
	*bssid_no = (addr_hi >> 24) & 1;
	*key_idx = addr_hi >> 30;

	int algo = (addr_hi >> 18) & 7;
	int subcfg = (addr_hi >> 16) & 3;
	if ((algo == 1 /* WPA_ALG_WEP40? */) && (subcfg == 1)) {
		algo = 5 /* WPA_ALG_WEP104? */;
	}
	*alg = algo;
	void *key_src = (void*)(&WDEV_KEYENTRY->E[key_entry_valid].DATA[0]);
	memcpy(key,key_src,key_len);
	return 0;
}

void
Tx_Copy2Queue(uint8_t index)
{
	/* Fairly sure index is 0-7, matching "our_instances" array size of 8 in lmacInit */
	int idx = (7 - index) & 7;
	WDEV->TXCFG[idx].CFG0 = WDEV->TX_CFG0 & 0x3fffffff;
	WDEV->TXCFG[idx].CFG1 = WDEV->TX_CFG1;
	WDEV->TXCFG[idx].CFG2 = WDEV->TX_CFG2;
	WDEV->TXCFG[idx].CFG3 = WDEV->TX_CFG3;
}

void
wDev_GetBAInfo(uint8_t *tid,uint16_t *ssn,uint32_t *low,uint32_t *high)
{
	/* Block Acknowledgement Frame Info */
	uint32_t tidssn = WDEV_BASE->BA_TIDSSN;
	/* TID ("Traffic Indicator", 802.11D user priority):
	 * 1 & 2: AC0 (AC_BK): Background   (lowest prio)
	 * 0 & 3: AC1 (AC_BE): Best Effort
	 * 4 & 5: AC2 (AC_VI): Video
	 * 6 & 7: AC3 (AC_VO): Voice        (highest prio)
	 */
	*tid = (tidssn >> 12) & 0xf;
	/* Starting Sequence Number (of first MSUD/A-MSDU/A-MPDU in * bitmap) */
	*ssn = tidssn & 0xfff;
	/* Block Ack Bitmap Low 32bit */
	*low = WDEV_BASE->BA_BITMAP_LO;
	/* Block Ack Bitmap High 32bit */
	*high = WDEV_BASE->BA_BITMAP_HI;
	/* Each bit in (compressed 64bit) bitmap indicates sucessful A-MPDU block */
	/* Uncompressed BA would be 128 octets */
}

uint32_t
wDev_GetTxqCollisions(void)
{
	return WDEV->TXQ_COLLISIONS & 0xfff;
}

void
wDev_ClearTxqCollisions(void)
{
	WDEV->TXQ_COLLISIONS &= ~0xfff;
}

void
wDev_ProcessCollision(uint8_t index)
{
	wDev_DisableTransmit(index);
	my_event.tx_collision_count[index]++;
}

void
wDev_AppendRxBlocks(lldesc_st *head,lldesc_st *tail,uint16_t num)
{
	uint16_t i;
	
	if (head == NULL) {
		i = 0;
	}
	else {
		i = 0;
		lldesc_st *lld = head;
		do {
			lld->owner = 1;
			lld->eof = 0;
			lld->sosf = 0;
			lld->length = lld->size;
			lld = lld->next;
			i++;
		} while (lld != NULL);
	}

	if (num != i) {
		ets_printf("AppendRxBlocks num %d != %d\n", num, i);
		while (1);
	}

	uint32_t saved = LOCK_IRQ_SAVE();
	int rx_remain_blocks = wDevCtrl.rx_remain_blocks;
	if (rx_remain_blocks == 0) {
		wDevCtrl.rx_head = head;
		WDEV_BASE->RX_HEAD = (uint32_t)head;
		wDevCtrl.rx_remain_blocks = wDevCtrl.rx_remain_blocks + num;
		wDevCtrl.rx_tail = tail;
		if (wDevCtrl.rx_remain_blocks == 1) {
			wDevCtrl.rx_tail->next = &wDevCtrl.fake_tail;
		}
	}
	else if (rx_remain_blocks == 1) {
		if (wDevCtrl.rx_head2 == NULL) {
			wDevCtrl.rx_head2 = head;
		}
		else {
			rx_remain_blocks = wDevCtrl.rx_remain_blocks2;
			wDevCtrl.rx_tail2->next = head;
			num = rx_remain_blocks + num;
		}
		wDevCtrl.rx_tail2 = tail;
		wDevCtrl.rx_remain_blocks2 = num;
	}
	else {
		wDevCtrl.rx_tail->next = head;
		wDevCtrl.rx_tail = tail;
		wDevCtrl.rx_remain_blocks = rx_remain_blocks + num;
	}
	if (1 < wDevCtrl.rx_remain_blocks + wDevCtrl.rx_remain_blocks2) {
		WDEV->ACK_TYPE[4] = WDEV->ACK_TYPE[4] & 0xfffffff | 0x10000000;
		WDEV->ACK_TYPE[5] = WDEV->ACK_TYPE[5] & 0xfffffff | 0x50000000;
	}
	LOCK_IRQ_RESTORE(saved);
}

void
wDev_AppendRxAmpduLensBlocks(lldesc_st *head,lldesc_st *tail,uint16_t num)
{
	uint16_t i;
	
	if (head == NULL) {
		i = 0;
	}
	else {
		i = 0;
		lldesc_st *lld = head;
		do {
			lld->owner = 1;
			lld->eof = 0;
			lld->sosf = 0;
			lld->length = lld->size;
			lld = lld->next;
			i++;
		} while (lld != NULL);
	}
	if (num != i) {
		ets_printf("AppendRxAmpduBlocks num %d != %d\n", num, i);
		while (1);
	}
	uint32_t saved = LOCK_IRQ_SAVE();
	if (wDevCtrl.rx_ampdu_len_head == NULL) {
		wDevCtrl.rx_ampdu_len_head = head;
		WDEV_BASE->RX_AMPDU_HEAD = (uint32_t)head;
	}
	else {
		wDevCtrl.rx_ampdu_len_tail->next = head;
	}
	wDevCtrl.rx_ampdu_len_tail = tail;
	wDevCtrl.rx_ampdu_len_remains += num;
	LOCK_IRQ_RESTORE(saved);
}

void
wDev_FetchRxLink(uint16_t nblks,lldesc_st *tail)
{
	uint16_t rx_remain_blocks2 = wDevCtrl.rx_remain_blocks2;
	wDevCtrl.rx_remain_blocks -= nblks;
	if (wDevCtrl.rx_remain_blocks < 2) {
		lldesc_st *lld;
		if (wDevCtrl.rx_remain_blocks == 1) {
			lld = wDevCtrl.rx_tail;
			wDevCtrl.rx_head = tail->next;
			tail->next = NULL;
			lld->next = &wDevCtrl.fake_tail;
		}
		else {
			wDevCtrl.rx_remain_blocks2 = 0;
			wDevCtrl.rx_remain_blocks = rx_remain_blocks2;
			lld = wDevCtrl.rx_tail2;
			wDevCtrl.rx_head = wDevCtrl.rx_head2;
			wDevCtrl.rx_head2 = NULL;
			wDevCtrl.rx_tail = lld;
			wDevCtrl.rx_tail2 = NULL;
			if (rx_remain_blocks2 == 1) {
				lld->next = &wDevCtrl.fake_tail;
			}
			tail->next = 0;
			WDEV_BASE->RX_HEAD = (uint32_t)wDevCtrl.rx_head;
		}
	}
	else {
		wDevCtrl.rx_head = tail->next;
		tail->next = NULL;
	}
	if (wDevCtrl.rx_remain_blocks + wDevCtrl.rx_remain_blocks2 < 2) {
		/* Read 3ff20ca8 / 3ff20cac and write them back unmodified */
		WDEV->ACK_TYPE[4] |= 0;
		WDEV->ACK_TYPE[5] |= 0;
	}
}

void
wDev_DiscardFrame(lldesc_st *tail,uint16_t nblks)
{
	lldesc_st *head = wDevCtrl.rx_head;
	wDev_FetchRxLink(nblks,tail);
	wDev_AppendRxBlocks(head,tail,nblks);
}

void
wDev_DiscardAMPDULen(lldesc_st *tail,uint16_t nblks)
{
	lldesc_st *head = wDevCtrl.rx_ampdu_len_head;
	wDevCtrl.rx_ampdu_len_head = tail->next;
	tail->next = NULL;
	wDevCtrl.rx_ampdu_len_remains -= nblks;
	wDev_AppendRxAmpduLensBlocks(head,tail,nblks);
}


void
wDev_IndicateFrame(ieee80211_channel_st *ch,bool isampdu,lldesc_st *tail,uint16_t nblks,uint32_t rxstart_time, int16_t chl_freq_offset)
{
	esf_buf_st *eb = esf_rx_buf_alloc(ESF_BUF_RX_BLOCK);
	if (eb == NULL) {
		wDev_DiscardFrame(tail,nblks);
		return;
	}
	esf_rx_desc_st *rxdesc = (eb->desc).rx_desc;
	rxdesc->channel = ch;
	rxdesc->timestamp = rxstart_time;

	eb->ds_len = nblks;
	lldesc_st *lldesc = wDevCtrl.rx_head;
	eb->ds_head = lldesc;
	eb->ds_tail = tail;
	eb->buf_begin = lldesc->buf;
	tail->eof = 0; /* *(uint *)tail = *(uint *)tail & 0xbfffffff; */
	eb->chl_freq_offset = chl_freq_offset;
	wDev_FetchRxLink(nblks,tail);
	lmacRxDone(eb);
}


void
wDev_ProcessRxSucData(lldesc_st *tail,uint16_t nblks,uint32_t rxstart_time,bool rxtime_valid)
{
	bool bVar1;
	int iVar2;
	int chl_freq_offset;
	uint32_t discard_frame;
	
	rx_control_st *rxctrl = (void*)wDevCtrl.rx_head->buf;
	int is_aggr = rxctrl->Aggregation;
	uint16_t frm_ctl = *(uint16_t *)(rxctrl + 1);

	ieee80211_channel_st *ch = chm_get_current_channel();
	if (ch == NULL) {
		ets_printf("ProcessRxSucDat ch is NULL\n");
		while (1);
	}
	rxctrl->channel = ch->ch_ieee;
	if ((rxctrl->rssi < -0x59) || (TestStaFreqCalValOK != true)) {
		chl_freq_offset = 0x7fff;
	}
	else {
		chl_freq_offset = phy_get_bb_freqoffset();
		if ((chl_freq_offset < 300) && (-300 < chl_freq_offset)) {
			chl_freq_offset += TestStaFreqCalValDev;
		}
	}
	if (is_aggr != 0) {
LAB_401025e6:
		bVar1 = false;
		discard_frame = 1;
		goto exit_cleanup;
	}
	if ((rxctrl->damatch0 == 0) && (rxctrl->damatch1 == 0)) {
		if ((rxctrl->bssidmatch0 == 0) && (rxctrl->bssidmatch1 == 0)) {
			bVar1 = false;
			discard_frame = g_ic.ic_mode != 1;
			goto exit_cleanup;
		}
		if ((frm_ctl & 0xf) == 0) {
			/* We got a mgmt frame */
			frm_ctl = frm_ctl & 0xff;
			if (frm_ctl == 0x40 /* Probe request */
				|| frm_ctl == 0xd0 /* Action */
				|| frm_ctl == 0xe0 /* Actio no ACK */) {
				discard_frame = 0;
			}
			else {
				if (frm_ctl != 0x50) {
					if (frm_ctl == 0x80) {
						/* Beacon */
						discard_frame = (rxtime_valid == '\0');
						if (rxtime_valid != '\0') {
							my_event.rx_bcn_count = my_event.rx_bcn_count + 1;
						}
						goto LAB_401026a9;
					}
				}
				/* Probe response */
				discard_frame = 1;
			}
LAB_401026a9:
			bVar1 = true;
		}
		else {
			if ((frm_ctl & 0xf) != 4) {
				if ((frm_ctl & 0xf) == 8) {
					/* Data frame */
					bVar1 = false;
					discard_frame = 0;
					goto LAB_401026ab;
				}
				/* Extension frame or bad protocol version */
				my_event.rx_type_error_count = my_event.rx_type_error_count + 1;
			}
			/* Control frame */
			bVar1 = false;
			discard_frame = 1;
		}
LAB_401026ab:
		my_event.rx_multicast_count = my_event.rx_multicast_count + 1;
		goto exit_cleanup;
	}
	if ((frm_ctl & 0xf) == 0) {
		/* We got a mgmt frame */
		if ((frm_ctl & 0xf0) == 0x80) {
			/* Beacon */
			if (((discard_frame >> 0x1c & 1) == 0) || ((discard_frame >> 0x1e & 1) == 0)) {
				if (((discard_frame >> 0x1d & 1) != 0) && ((int)discard_frame < 0)) goto LAB_401026d9;
				discard_frame = 1;
			}
			else {
				discard_frame = (rxtime_valid == '\0');
			}
		}
		else {
LAB_401026d9:
			discard_frame = 0;
		}
		bVar1 = true;
		my_event.rx_my_management_count = my_event.rx_my_management_count + 1;
		goto exit_cleanup;
	}
	if ((frm_ctl & 0xf) != 4) {
		if ((frm_ctl & 0xf) == 8) {
			/* Data frame */
			my_event.rx_my_data_count = my_event.rx_my_data_count + 1;
			rcUpdateDataRxDone(rxctrl);
			bVar1 = false;
			discard_frame = 0;
			goto exit_cleanup;
		}
		/* Extension frame or bad protocol version */
		my_event.rx_type_error_count = my_event.rx_type_error_count + 1;
		goto LAB_401025e6;
	}
	frm_ctl = frm_ctl & 0xf0;
	if (frm_ctl == 0x80) {
		/* Block ack request */
		my_event.rx_bar_count = my_event.rx_bar_count + 1;
LAB_401026fb:
		discard_frame = 1;
	}
	else {
		if (frm_ctl == 0x90) {
			/* Block ack */
			my_event.rx_ba_count = my_event.rx_ba_count + 1;
			goto LAB_401026fb;
		}
		if (frm_ctl != 0xa0) {
			my_event.rx_other_control_count = my_event.rx_other_control_count + 1;
			goto LAB_401026fb;
		}
		/* PS-Poll */
		discard_frame = 0;
		my_event.rx_pspoll_count = my_event.rx_pspoll_count + 1;
	}
	bVar1 = false;

exit_cleanup:
	if ((wDevCtrl.rx_remain_blocks + wDevCtrl.rx_remain_blocks2 < 2) && (!bVar1)) {
		discard_frame = 1;
	}
	if (WdevRxIsClose) {
		discard_frame = 1;
	}
	if (discard_frame == 0) {
		if (tail->length == 0) {
			ets_printf("ProcessRxSucData: tail len 0\n");
			while (1);
		}
		wDev_IndicateFrame(ch,is_aggr,tail,nblks,rxstart_time,chl_freq_offset);
	}
	else {
		wDev_DiscardFrame(tail,nblks);
	}
}

struct rx_ampdu_len {
	rxend_state_t substate;
	uint32_t sublen:12;
};
typedef struct rx_ampdu_len rx_ampdu_len_st;
static_assert(sizeof(rx_ampdu_len_st) == 4, "rx_ampdu_len size mismatch");

struct ampdu_len_seq {
	uint16_t len;
	uint16_t seq;
	uint8_t addr3[6];
};
typedef struct ampdu_len_seq ampdu_len_seq_st;
static_assert(sizeof(ampdu_len_seq_st) == 10, "ampdu_len_seq_st size mismatch");

struct sniffer_buf {
	rx_control_st rx_ctrl;
	uint8_t data[36];
	uint16_t cnt;
	struct ampdu_len_seq lenseq[0];
};
typedef struct sniffer_buf sniffer_buf_st;

struct sniffer_buf2 {
	rx_control_st rx_ctrl;
	uint8_t data[112];
	uint16_t cnt;
	uint16_t len;
};
typedef struct sniffer_buf2 sniffer_buf2_st;

void
wDev_SnifferRxData2(lldesc_st *head)
{
	uint16_t uVar1;
	ieee80211_channel_st *ch;

	lldesc_st *rx_ampdu_len_head = wDevCtrl.rx_ampdu_len_head;
	lldesc_st *rx_head = wDevCtrl.rx_head;
	rx_control_st *rxc = (void*)rx_head->buf;
	ieee80211_frame_st *frame = (void *)(rxc + 1);
	uint16_t nblks = 1;

	if (rx_head->eof == 0) {
		nblks = 1;
		do {
			rx_head = rx_head->next;
			nblks++;
		} while (rx_head->eof == 0);
	}
	if (rxc->Aggregation != 0) {
		sniffer_buf_st *msg = os_malloc(sizeof(*msg) + rxc->ampdu_cnt * sizeof(ampdu_len_seq_st));
		if (msg == NULL) {
			wDev_DiscardFrame(rx_head,nblks);
			wDev_DiscardAMPDULen(wDevCtrl.rx_ampdu_len_head,1);
			return;
		}
		memcpy(msg,rxc,sizeof(*msg)+sizeof(ampdu_len_seq_st));
		ampdu_len_seq_st *lenseq = msg->lenseq;
		msg->cnt = 0;
		rx_ampdu_len_st *rxampdulen = (rx_ampdu_len_st *)rx_ampdu_len_head->buf;
		do {
			if (((rx_ampdu_len_head->buf + rx_ampdu_len_head->length) <= (uint8_t*)rxampdulen) || (rxc->ampdu_cnt <= msg->cnt))
				goto LAB_401034eb;
			uint8_t substate = rxampdulen->substate;
			if ((substate == RX_OK) || ((RX_SECOV_ERR <= substate && (substate <= RX_WAPIMIC_ERR)))) {
				if (wDevCtrl.rx_head->buf + wDevCtrl.rx_head->size <= (uint8_t*)(frame + 1))
					goto LAB_401034eb;
				msg->cnt = msg->cnt + 1;
										/* WARNING: Load size is inaccurate */
				lenseq->len = rxampdulen->sublen;
				/* frame points to the 802.11 frame header:
				 *  0: frame control (2 octets)
				 *  2: duration / id (2 octets)
				 *  4: address1
				 * 10: address2
				 * 16: address3
				 * 22: sequence control (2 octets)
				 */
				lenseq->seq = *(uint16_t *)frame->i_seq;
				memcpy(lenseq->addr3,frame->i_addr3,6);
				int frame_len = 0x24 /* 36 */;
				lenseq = lenseq + 1;
				if ((frame->i_fc[1] & 3) != 3) {
					frame_len = 0x1e /* 30 */;
				}
				if ((char)frame->i_fc[1] < 0) {
					frame_len += 4;
				}
				/* round up to next 32bit word */
				frame_len = (frame_len + 3) & ~3;
				frame = (ieee80211_frame_st*)((uint8_t*)frame + frame_len);
			}
			rxampdulen = rxampdulen + 1;
		} while( true );
	}
	bool is_mgmt = (*(uint8_t *)(rxc + 1) & 0xf) == 0;
	sniffer_buf_st *msg;
	if (is_mgmt) {
		msg = os_malloc(0x80);
	}
	else {
		msg = os_malloc(0x3c);
	}
	if (msg == NULL) {
		wDev_DiscardFrame(rx_head,nblks);
		return;
	}

	if (is_mgmt) {
		sniffer_buf2_st *msg2 = (void*)msg;
		memcpy(msg,rxc,0x80);
		msg2->cnt = 1;
		if (rxc->sig_mode == 0) {
			msg2->len = rxc->legacy_length;
		}
		else {
			msg2->len = rxc->HT_length;
		}
	}
	else {
		memcpy(msg,rxc,0x3c);
		msg->cnt = 1;
		if (rxc->sig_mode == 0) {
			msg->lenseq[0].len = rxc->legacy_length;
		}
		else {
			msg->lenseq[0].len = rxc->HT_length;
		}
		/* Doesn't match the frame offset used earlier, extra 12 bytes (2 hw addrs?) ? */
		msg->lenseq[0].seq = *(uint16_t *)((uint8_t*)rxc + 0x22);
		memcpy(msg->lenseq[0].addr3,(uint8_t*)rxc + 0x1c,6);
	}
	wDev_DiscardFrame(rx_head,nblks);
LAB_401033ed:
	ch = chm_get_current_channel();
	if (ch != NULL) {
		msg->rx_ctrl.channel = ch->ch_ieee;
		if (pp_post2(PRIO_PP,9,(ETSParam)msg) != OK) {
			os_free(msg);
		}
		return;
	}
	ets_printf("SnifferRxData2 ch is NULL\n");
	while (1);

LAB_401034eb:
	wDev_DiscardFrame(rx_head,nblks);
	wDev_DiscardAMPDULen(wDevCtrl.rx_ampdu_len_head,1);
	goto LAB_401033ed;
}



void
wDev_SnifferRxHT40(void)
{
	int rxstart_time = WDEV_TIMER->RXSTART_TIME;
	int t_now = WDEV->MICROS;
	int delta = t_now - rxstart_time;
	while (delta < 30) {
		if ((WDEV->INT_RAW & 4) != 0) {
			return;
		}
		t_now = WDEV->MICROS;
		delta = t_now - rxstart_time;
	}
	uint32_t r1d4 = WDEV_BASE->REG1D4;
	if (7 < (r1d4 >> 8 & 0xff)) {
		WDEV->CTL_C70 &= ~2;

		/* Wait for interrupt bit 2 */
		while ((WDEV->INT_RAW & 4) == 0);
		/* And ack it */
		WDEV->INT_ACK = 4;

		uint32_t r038 = WDEV_BASE->reg038;
		if (((r038 & 0xff) < 0x42) || ((r038 & 0xff) >> 7 != 0)) {
			rx_control_st *msg = os_malloc(sizeof(rx_control_st));
			if (msg != NULL) {
				memcpy(msg,wDevCtrl.rx_head->buf,0xc);
				ieee80211_channel_st *chan = chm_get_current_channel();
				if (chan == NULL) {
					ets_printf("SnifferRxHT40 chan NULL");
					while(1);
				}
				msg->channel = chan->ch_ieee;
				if ((msg->sig_mode == 0) || (msg->CWB == 0 && (msg->MCS < 8)) || (0x1f < msg->MCS)) {
					os_free(msg);
				}
				else {
					if (pp_post2(PRIO_PP,9,(ETSParam)msg) != OK) {
						os_free(msg);
					}
				}
			}
		}
		WDEV->CTL_C70 |= 2;
	}
}


void
wDev_SnifferRxLDPC(void)
{
	/* LDPC -> Low-Density-Parity-Check message (FEC_CODING == 1) */
	uint32_t uVar1 = WDEV_BASE->reg038;

	if (!(((uVar1 & 0xff) < 0x42) || ((uVar1 & 0xff) >> 7 != 0))) {
		return;
	}

	rx_control_st *msg;
	rx_control_st *rxc = (void*)wDevCtrl.rx_head->buf;
	if (((rxc->sig_mode != 0) && (((rxc->CWB != 0 || (7 < rxc->MCS)) || ((rxc->FEC_CODING & 1) != 0)))) && (msg = os_malloc(0xc), msg != NULL)) {
		memcpy(msg,rxc,0xc);
		ieee80211_channel_st *chan = chm_get_current_channel();
		if (chan == NULL) {
			ets_printf("SnifferRxLDPC chan is NULL\n");
			while (1);
		}
		msg->channel = chan->ch_ieee;
		if (msg->sig_mode != 0) {
			uVar1 = *(uint32_t *)((uint16_t*)rxc + 2); /* offset 4 */
			uVar1 = *(uint32_t *)(msg + 2);
			if ((((msg->CWB != 0) || (7 < msg->MCS)) || (msg->FEC_CODING != 0)) && (msg->MCS < 0x20)) {
				if (pp_post2(PRIO_PP,9,(ETSParam)msg) != OK) {
					os_free(msg);
				}
				return;
			}
		}
		os_free(msg);
	}
}

void
relib_wDev_ProcessFiq(void *unused_arg)
{
	bool bVar1;
	lldesc_st *rx_desc;
	lldesc_st *tail;
	uint32_t rxstart_time;
	uint8_t ack_snr;
	lldesc_st *plVar2;
	lldesc_st *rx_head;
	uint8_t index;
	uint16_t nblks;
	lldesc_st *in_a11;
	uint32_t tx_queue;
	lldesc_st *plStack_34;
	uint32_t uStack_24;
	uint8_t tx_error;
	uint32_t int_status_;
	uint32_t error_type;
	uint32_t int_status;
	uint32_t maybeTxRxState;
	uint32_t old_int_ena;
	
	maybeTxRxState = WDEV->TXRX_STATE;
	int_status = WDEV->INT_STATUS;
	old_int_ena = WDEV->INT_ENA;
	WDEV->INT_ENA = 0;
	WDEV->INT_ACK = int_status;
	if (int_status != 0) {
		if ((int_status >> 0x1c & 1) != 0) {
			ets_printf("%s %u\n","dev",0x448);
			do {
										/* WARNING: Do nothing block with infinite loop */
			} while( true );
		}
		if ((int_status >> 0x1b & 1) != 0) {
			wDev_MacTimerISRHdl('\0');
		}
		if ((int_status >> 0x1a & 1) != 0) {
			wDev_MacTimerISRHdl('\x01');
		}
		if (((int_status >> 3 & 1) == 0) || ((int_status >> 2 & 1) != 0)) {
			if (((int_status & 4) != 0) && ((int_status & 0x100) == 0)) {
				wDev_SnifferRxLDPC();
			}
		}
		else {
			wDev_SnifferRxHT40();
		}
		rxstart_time = WDEV_TIMER->RXSTART_TIME;
		tail = rx_desc = (lldesc_st*) WDEV_BASE->RX_DESC;
		if ((int_status & 0x100) != 0) {
			if (wDevCtrl.sniffer_mode == '\0') {
				if (rx_desc == NULL) {
					ets_printf("%s %u\n","dev",0x48a);
					do {
										/* WARNING: Do nothing block with infinite loop */
					} while( true );
				}
				rx_head = wDevCtrl.rx_head;
				plStack_34 = in_a11;
				if (rx_desc->next == NULL) {
					ets_printf("%s %u\n","dev",0x48b,WDEV_BASE->reg01c);
					do {
										/* WARNING: Do nothing block with infinite loop */
					} while( true );
				}
				do {
					nblks = 1;
					for (; rx_head != (lldesc_st *)0x0; rx_head = rx_head->next) {
						if ((*(uint32_t *)rx_head & 0x7fffffff) >> 0x1e != 0) {
							if (rx_head != NULL) {
								plStack_34 = rx_head->next;
							}
							break;
						}
						nblks = nblks + 1 & 0xff;
					}
					if (nblks < 2) {
						wDev_ProcessRxSucData(rx_head,1,rxstart_time,tail == rx_head);
						my_event.rx_suc_data_count = my_event.rx_suc_data_count + 1;
					}
					else {
						if (rx_head == (lldesc_st *)0x0) {
							if ((*(uint32_t *)tail & 0x7fffffff) >> 0x1e != 0) {
								wDev_DiscardFrame(tail,nblks);
							}
							break;
						}
						wDev_DiscardFrame(rx_head,nblks);
					}
					bVar1 = tail != rx_head;
					rx_head = plStack_34;
				} while (bVar1);
			}
			else {
				if (rx_desc == NULL) {
					ets_printf("%s %u\n","dev",0x469);
					do {
										/* WARNING: Do nothing block with infinite loop */
					} while( true );
				}
				if (rx_desc->next == NULL) {
					ets_printf("%s %u\n","dev",0x46a);
					do {
										/* WARNING: Do nothing block with infinite loop */
					} while( true );
				}
				rx_head = wDevCtrl.rx_head;
				if ((*(uint32_t *)rx_desc & 0x7fffffff) >> 0x1e != 1) { /* Check bit 30 is set */
					ets_printf("%s %u\n","dev",0x46b,WDEV_BASE->reg01c, WDEV_BASE->reg018,WDEV_TIMER->RXSTART_TIME);
					do {
										/* WARNING: Do nothing block with infinite loop */
					} while( true );
				}
				while (rx_head != (lldesc_st *)0x0) {
					tx_queue = *(uint32_t *)rx_head;
					plVar2 = rx_head;
					while ((tx_queue & 0x7fffffff) >> 0x1e == 0) {
						plVar2 = plVar2->next;
						tx_queue = *(uint32_t *)plVar2;
					}
					wDev_SnifferRxData2(rx_head);
					my_event.rx_suc_data_count = my_event.rx_suc_data_count + 1;
					if (tail == plVar2) break;
					rx_head = plVar2->next;
				}
			}
		}
		if ((((int_status & 0x30000) != 0) && ((int_status >> 0x13 & 1) != 0)) &&
			 (tx_queue = maybeTxRxState >> 0xc & 0xf, tx_queue == (maybeTxRxState >> 8 & 0xf))) {
			lmacProcessRtsStart((u8)tx_queue);
		}
		int_status_ = int_status;
		if ((int_status & 0x80000) == 0) {
			uStack_24 = int_status & 0x40000;
		}
		else {
			uint32_t reg_snr = WDEV->ACK_SNR;
			if (((reg_snr >> 0x18) & 1) == 0) {
				ack_snr = '\x7f';
			}
			else {
				ack_snr = reg_snr >> 0x10;
			}
			tx_queue = maybeTxRxState >> 0xc & 0xf;
			if ((7 < tx_queue) && (tx_queue != 10)) {
				ets_printf("%s %u\n","dev",0x4c9);
				do {
										/* WARNING: Do nothing block with infinite loop */
				} while( true );
			}
			uStack_24 = int_status & 0x40000;
			index = (uint8_t)tx_queue;
			if ((our_tx_eb == NULL) && ((int_status & 0x40000) != 0)) {
				lmacProcessTXStartData(index);
				int_status_ = int_status & 0xfffbffff;
				uStack_24 = 0;
			}
			error_type = maybeTxRxState >> 0x1c;
			if (error_type == 0) {
				lmacProcessTxSuccess(index,ack_snr);
			}
			else {
				tx_error = (u8)(maybeTxRxState >> 0x10);
				if (error_type == 1) {
					lmacProcessTxRtsError(tx_error,index);
				}
				else if (error_type == 2) {
					lmacProcessCtsTimeout(index);
				}
				else if (error_type == 4) {
					lmacProcessTxError(tx_error);
				}
				else {
					if (error_type != 5) {
						ets_printf("%s %u\n","dev",0x4e4);
						do {
										/* WARNING: Do nothing block with infinite loop */
						} while( true );
					}
					lmacProcessAckTimeout();
				}
			}
			my_event.tx_queue_counts[tx_queue] = my_event.tx_queue_counts[tx_queue] + 1;
		}
		int_status = int_status & 0x30000;
		if (uStack_24 != 0) {
			tx_queue = maybeTxRxState >> 0xc & 0xf;
			if ((7 < tx_queue) && (tx_queue != 10)) {
				ets_printf("%s %u\n","dev",0x4eb);
				do {
										/* WARNING: Do nothing block with infinite loop */
				} while( true );
			}
			lmacProcessTXStartData((uint8_t)tx_queue);
		}
		if ((int_status_ >> 0x1d & 1) != 0) {
			lmacProcessAllTxTimeout();
		}
		if ((int_status_ >> 0x14 & 1) != 0) {
			lmacProcessCollisions();
		}
		if ((int_status_ >> 9 & 1) != 0) {
			my_event.rx_hung_count = my_event.rx_hung_count + 1;
		}
		if ((int_status_ >> 0x17 & 1) != 0) {
			my_event.panic_reset_count = my_event.panic_reset_count + 1;
		}
		if ((int_status != 0) &&
			 ((tx_queue = maybeTxRxState >> 0xc & 0xf, (int_status_ & 0x80000) == 0 ||
				(tx_queue != (maybeTxRxState >> 8 & 0xf))))) {
			lmacProcessRtsStart((u8)tx_queue);
		}
	}
	WDEV->INT_ENA = old_int_ena;
}

void ICACHE_FLASH_ATTR
wDev_Set_Beacon_Int(uint32_t beacon_int)
{
	BcnInterval = beacon_int;
}

void ICACHE_FLASH_ATTR
wDev_Disable_Beacon_Tsf(void)
{
	WDEV_TIMER->MAC_TIM1_CTL2 &= ~0xc4000000;
}

void ICACHE_FLASH_ATTR
wDev_Enable_Beacon_Tsf(void)
{
	WDEV_TIMER->MAC_TIM1_CTL2 |= 0xc4000000;
}

uint32_t ICACHE_FLASH_ATTR
wDev_Get_Next_TBTT(void)
{
	int32_t now;
	int32_t delta;

	do {
		BcnSendTick = BcnSendTick + BcnInterval;
		now = WDEV_TIMER->MAC_TIM1_COUNT_LO;
		delta = BcnSendTick - now;
	} while (BcnInterval < delta);
	return (delta / 1000) + 1;
}

void ICACHE_FLASH_ATTR
wDev_Reset_TBTT(void)
{
	wDev_Disable_Beacon_Tsf();
	PPWdtReset();
	BcnSendTick = 0;
	WDEV_TIMER->MAC_TBTT_REG5C = 0;
	WDEV_TIMER->MAC_TBTT_REG60 = 0;
	wDev_Enable_Beacon_Tsf();
}

void ICACHE_FLASH_ATTR
wDev_SetMacAddress(uint8_t index,uint8_t *address)
{
	uint32_t mac_lo = address[0]
		| ((uint32_t)address[1] << 8)
		| ((uint32_t)address[2] << 16)
		| ((uint32_t)address[3] << 24);
	uint32_t mac_hi = address[4] | ((uint32_t)address[5] << 8);

	if (index -= 0) {
		WDEV->MAC0_LO = mac_lo;
		WDEV->MAC0_HI = mac_hi;
		WDEV->MAC0_MASK_LO = 0xffffffff;
		WDEV->MAC0_MASK_HI = 0xffff;
		WDEV->MAC0_MASK_HI |= 0x10000;
	} else {
		WDEV->MAC1_LO = mac_lo;
		WDEV->MAC1_HI = mac_hi;
		WDEV->MAC1_MASK_LO = 0xffffffff;
		WDEV->MAC1_MASK_HI = 0xffff;
		WDEV->MAC1_MASK_HI |= 0x10000;
	}
}

void ICACHE_FLASH_ATTR
wDev_SetBssid(uint8_t index,const uint8_t *bssid)
{
	uint32_t bss_lo = bssid[0]
		| ((uint32_t)bssid[1] << 8)
		| ((uint32_t)bssid[2] << 16)
		| ((uint32_t)bssid[3] << 24);
	uint32_t bss_hi = bssid[4] | ((uint32_t)bssid[5] << 8);
	
	if (index != 0) {
		WDEV->MAC1_BSSID_MASK_HI &= ~0x10000;
		WDEV->MAC1_BSSID_LO = bss_lo;
		WDEV->MAC1_BSSID_HI = bss_hi;
		WDEV->MAC1_BSSID_MASK_LO = 0xffffffff;
		WDEV->MAC1_BSSID_MASK_HI = 0xffff;
		WDEV->MAC1_BSSID_MASK_HI |= 0x10000;
	} else {
		WDEV->MAC0_BSSID_MASK_HI &= ~0x10000;
		WDEV->MAC0_BSSID_LO = bss_lo;
		WDEV->MAC0_BSSID_HI = bss_hi;
		WDEV->MAC0_BSSID_MASK_LO = 0xffffffff;
		WDEV->MAC0_BSSID_MASK_HI = 0xffff;
		WDEV->MAC0_BSSID_MASK_HI |= 0x10000;
	}
}

void ICACHE_FLASH_ATTR
wDev_EnableUcRx(uint8_t index)
{
	if (index == 0) {
		WDEV->MAC0_MASK_HI |= 0x10000;
	} else {
		WDEV->MAC1_MASK_HI |= 0x10000;
	}
}

void ICACHE_FLASH_ATTR
wDev_DisableUcRx(uint8_t index)
{
	if (index == 0) {
		WDEV->MAC0_MASK_HI &= ~0x10000;
	} else {
		WDEV->MAC1_MASK_HI &= ~0x10000;
	}
}

void ICACHE_FLASH_ATTR
wDev_remove_KeyEntry(int key_entry_valid)
{
	WDEV_CRYPTO->KEYENTRY_ENABLE &= ~(1 << key_entry_valid);
	WDEV_KEYENTRY->E[key_entry_valid].ADDR_LO = 0;
	WDEV_KEYENTRY->E[key_entry_valid].ADDR_HI = 0;
	WDEV_KEYENTRY->E[key_entry_valid].DATA[0] = 0;
	WDEV_KEYENTRY->E[key_entry_valid].DATA[1] = 0;
	WDEV_KEYENTRY->E[key_entry_valid].DATA[2] = 0;
	WDEV_KEYENTRY->E[key_entry_valid].DATA[3] = 0;
	WDEV_KEYENTRY->E[key_entry_valid].DATA[4] = 0;
	WDEV_KEYENTRY->E[key_entry_valid].DATA[5] = 0;
	WDEV_KEYENTRY->E[key_entry_valid].DATA[6] = 0;
	WDEV_KEYENTRY->E[key_entry_valid].DATA[7] = 0;
}

void ICACHE_FLASH_ATTR
wDev_remove_KeyEntry_all_cnx(uint8_t ifidx)
{
	uint32_t bssid_key_mask = wDevCtrl.bssid_key_mask[ifidx];
	uint32_t key_mask = bssid_key_mask & ~g_ic.avs_key_mask;

	for (int key_entry_valid = 0; key_entry_valid < 0x19; key_entry_valid++) {
		if (key_mask & (1 << key_entry_valid)) {
			wDev_remove_KeyEntry(key_entry_valid);
		}
	}
}


void ICACHE_FLASH_ATTR
wDev_Insert_KeyEntry(uint8_t alg,uint8_t bssid_no,int key_idx,const uint8_t *addr,int key_entry_valid,const uint8_t *key, int key_len)
{
	uint32_t mask = 1 << key_entry_valid;
	bool is_alg5 = alg == 5;
	int type;
	if (alg == 5 /* WEP104? */) {
		alg = 1 /* WEP40 */;
	}
	if (alg == 1 /* WEP40? */ ) {
		type = 7;
	}
	else {
		type = 6;
		if (5 < key_entry_valid) {
			type = 2;
		}
	}
	if (bssid_no < 2) {
		if ((1 < key_entry_valid) && ((g_ic.avs_key_mask & mask) != 0)) {
			type = 3;
		}
	}
	if (key_entry_valid == 2) {
		uint32_t entry3_idx = WDEV_KEYENTRY->E[3].ADDR_HI >> 30;
		if (key_idx == entry3_idx) {
			wDev_remove_KeyEntry(3);
		}
	}
	WDEV_KEYENTRY->E[key_entry_valid].ADDR_LO = addr[0]
		| ((uint32_t)addr[1] << 8)
		| ((uint32_t)addr[2] << 16)
		| ((uint32_t)addr[3] << 24);
	WDEV_KEYENTRY->E[key_entry_valid].ADDR_HI = addr[4]
		| ((uint32_t)addr[5] << 8)
		| ((uint32_t)is_alg5 << 16)
		| ((uint32_t)(alg & 7) << 18)
		| ((uint32_t)type << 21)
		| ((uint32_t)(bssid_no & 1) << 24)
		| ((uint32_t)key_idx << 30);

	WDEV_CRYPTO->KEYENTRY_ENABLE |= mask;
	if ((bssid_no < 2) && (1 < key_entry_valid)) {
		wDevCtrl.bssid_key_mask[bssid_no] |= mask;
	}
	memcpy((void*)WDEV_KEYENTRY->E[key_entry_valid].DATA,key,key_len);
}


void ICACHE_FLASH_ATTR
wDev_Crypto_Conf(uint8_t index,uint8_t alg)
{
	if (index == 0) {
		WDEV_CRYPTO->MAC0_CRYPTO_CONF = 0x30303;
		if ((alg == 1 /* WEP40? */) || (alg == 5 /* WEP104? */)) {
			WDEV_CRYPTO->MAC0_CRYPTO_CONF |= 0x10000000;
		}
	}
	else {
		WDEV_CRYPTO->MAC1_CRYPTO_CONF = 0x30303;
		if ((alg == 1 /* WEP40? */) || (alg == 5 /* WEP104? */)) {
			WDEV_CRYPTO->MAC1_CRYPTO_CONF |= 0x10000000;
		}
	}
}

void ICACHE_FLASH_ATTR
wDev_Crypto_Disable(uint8_t index)
{
	if (index == 0) {
		WDEV_CRYPTO->MAC0_CRYPTO_CONF = 0x30000;
	}
	else {
		WDEV_CRYPTO->MAC1_CRYPTO_CONF = 0x30000;
	}
	WDEV_CRYPTO->KEYENTRY_ENABLE &= ~wDevCtrl.bssid_key_mask[index];
}

void ICACHE_FLASH_ATTR
wDev_SetRxPolicy(bcn_policy_t policy,uint8 index,uint8 *bssid)
{
	if (policy == RX_DISABLE) {
		if (index == 0) {
			WDEV->MAC0_BSSID_MASK_HI &= ~0x10000;
		}
		else {
			WDEV->MAC1_BSSID_MASK_HI &= ~0x10000;
		}
	}
	else if (policy == RX_ANY) {
		wDev_SetBssid(index,ethbroadcast.addr);
	}
	else if ((policy == RX_BSSID) && (bssid != NULL)) {
		wDev_SetBssid(index,bssid);
	}
}

void ICACHE_FLASH_ATTR
wDev_Option_Init(void)
{
	WDEV->regc88 |= 0x8084a000;
	WDEV->regc88 &= 0xffdfbff7;
	WDEV->regc90 |= 8;
	WDEV->regc94 |= 3;

	WDEV->rege08 &= 0xffffff0f;

	uint32_t x = WDEV->regc68;
	WDEV->regc68 = x & 0xff00ffff | ((x + 0x120000) & 0xff0000);

	WDEV->regc6c = WDEV->regc6c & 0xffffff00 | 0x16;
	WDEV->regc6c = WDEV->regc6c & 0xffff00ff | 0x1600;

	WDEV->regc14 = WDEV->regc14 & 0xfffff000 | 0xf0;
	WDEV->regc14 |= 0x80000000;
	WDEV->regc14 |= 0x40000000;
}


void ICACHE_FLASH_ATTR
wDev_Init_dummy_key(void)
{
	WDEV_CRYPTO->MAC0_CRYPTO_CONF = 0x30000;
	WDEV_CRYPTO->MAC1_CRYPTO_CONF = 0x30000;
	wDev_Insert_KeyEntry(3,0,0,ethbroadcast.addr,0,ethbroadcast.addr,6);
	wDev_Insert_KeyEntry(3,1,0,ethbroadcast.addr,1,ethbroadcast.addr,6);
	WDEV_CRYPTO->REG008 = 0;
}

void lldesc_build_chain(lldesc_st *descptr, uint32_t desclen, uint8_t *mblkptr, uint32_t buflen, uint32_t blksz, bool owner, lldesc_st **head, lldesc_st **tail);

void ICACHE_FLASH_ATTR
wDev_Rxbuf_Init(void)
{
	lldesc_build_chain(
		_wdev_rx_desc_space,sizeof(_wdev_rx_desc_space),
		_wdev_rx_mblk_space,sizeof(_wdev_rx_mblk_space),RX_MBLK_ENTRY_LEN,
		1,&wDevCtrl.rx_head,&wDevCtrl.rx_tail);

	lldesc_st *desc = wDevCtrl.rx_head;
	wDevCtrl.rx_remain_blocks = 6;
	for (; desc != NULL; desc = desc->next) {
		desc->size -= 4;
		desc->length = desc->size;
		wDevCleanRxBuf(desc);
	}
	lldesc_build_chain(
		_wdev_tx_desc_space,sizeof(_wdev_tx_desc_space),
		_wdev_tx_mblk_space,sizeof(_wdev_tx_mblk_space),TX_MBLK_ENTRY_LEN,
		0,&wDevCtrl.tx_head, &wDevCtrl.tx_tail);
	(wDevCtrl.fake_tail).next = 0;
	wDevCtrl.rx_remain_blocks2 = 0;
	(wDevCtrl.fake_tail).buf = wDevCtrl.fake_buf;
	wDevCtrl.fake_tail.size = 4;
	wDevCtrl.fake_tail.owner = 1;
	wDevCtrl.fake_tail.sosf = 0;
	wDevCtrl.fake_tail.eof = 0;
	lldesc_build_chain(
		_wdev_rx_ampdu_len_desc_space,sizeof(_wdev_rx_ampdu_len_desc_space),
		_wdev_rx_ampdu_len_space,sizeof(_wdev_rx_ampdu_len_space),RX_AMPDU_ENTRY_LEN,
		1, &wDevCtrl.rx_ampdu_len_head,&wDevCtrl.rx_ampdu_len_tail);
	wDevCtrl.rx_ampdu_len_remains = 10;
	WDEV_BASE->RX_DESC_START = (uint32_t)_wdev_rx_desc_space;
	WDEV_BASE->RX_MBLK_END = (uint32_t)&_wdev_rx_mblk_space[sizeof(_wdev_rx_mblk_space)];
	WDEV_BASE->RX_AMPDU_DESC_START = (uint32_t)_wdev_rx_ampdu_len_desc_space;
	WDEV_BASE->RX_AMPDU_MBLK_END = (uint32_t)&_wdev_rx_ampdu_len_space[sizeof(_wdev_rx_ampdu_len_space)];

	WDEV_BASE->CTL0 &= 0xffffff00;
	WDEV_BASE->RX_HEAD = (uint32_t)wDevCtrl.rx_head;
	WDEV_BASE->RX_AMPDU_HEAD = (uint32_t)&wDevCtrl.fake_tail;
	wDevCtrl.sniffer_mode = 0;
	WDEV_BASE->CTL10 = 0;
	WDEV_BASE->CTL0 &= 0xdfffffff;
}

void ICACHE_FLASH_ATTR
wDev_AutoAckRate_Init(void)
{
	WDEV_BASE->AUTOACKRATE0 = 0x76503210;
	WDEV_BASE->AUTOACKRATE1 = 0xbbbbbbbb;
	WDEV_BASE->AUTOACKRATE2 = 0xbbbbbbbb;
}

void ICACHE_FLASH_ATTR
wDev_Bssid_Init(void)
{
	WDEV_BASE->reg06c_recv_flag |= 0x0707;
	WDEV_BASE->reg06c_recv_flag &= ~0x0010;
	WDEV_BASE->reg06c_recv_flag &= ~0x1000;
	wDev_SetRxPolicy(RX_DISABLE,0,NULL);
	wDev_SetRxPolicy(RX_DISABLE,1,NULL);
}


void ICACHE_FLASH_ATTR
wDev_Initialize(void)
{
	WDEV->INT_ENA = 0;
	WDEV->INT_ACK = 0xffffffff;
	wDev_Option_Init();
	wDev_Init_dummy_key();
	wDev_Rxbuf_Init();
	wDev_AutoAckRate_Init();
	wDev_Bssid_Init();
	WDEV->INT_ENA = 0x2c9f0300;
	WDEV_BASE->REG178 |= 2;
	WDEV_BASE->RX_CTL &= ~0x80000000;
	WdevRxIsClose = true;
}

/*
         U chm_get_current_channel
         U esf_rx_buf_alloc
         U ethbroadcast
         U ets_delay_us
         U ets_intr_lock
         U ets_intr_unlock
         U ets_memcmp
         U ets_memcpy
         U ets_printf
         U g_ic
         U lldesc_build_chain
         U lmacProcessAckTimeout
         U lmacProcessAllTxTimeout
         U lmacProcessCollisions
         U lmacProcessCtsTimeout
         U lmacProcessRtsStart
         U lmacProcessTxError
         U lmacProcessTxRtsError
         U lmacProcessTXStartData
         U lmacProcessTxSuccess
         U lmacRxDone
         U our_tx_eb
         U phy_get_bb_freqoffset
         U pp_post2
         U PPWdtReset
         U pvPortMalloc
         U rcUpdateDataRxDone
         U roundup2
         U TestStaFreqCalValDev
         U TestStaFreqCalValOK
         U __udivsi3
         U vPortFree
00000000 d g_wDevCtrl_83
00000000 t mem_debug_file_178
00000000 B wDevCtrl
00000004 d BcnInterval_108
00000004 t wDevCleanRxBuf
00000004 T wDev_GetBAInfo
00000004 T wDev_GetTxqCollisions
00000004 t wDev_MacTimerISRHdl
00000004 T wDev_SetFrameAckType
00000008 T Tx_Copy2Queue
00000008 T wDev_ClearTxqCollisions
00000008 T wDev_ClearWaitingQueue
00000008 T wDev_DisableTransmit
00000008 T wDev_ProcessCollision
0000000c T wDevDisableRx
0000000c t wDev_DiscardAMPDULen
0000000c t wDev_DiscardFrame
0000000c T wDev_EnableTransmit
0000000c T wDev_Get_KeyEntry
0000000c T wDev_MacTim1SetFunc
0000000c T wDev_SetWaitingQueue
00000010 t wDev_FetchRxLink
00000010 T wDev_MacTim1Arm
00000018 t wDev_IndicateFrame
00000028 T wDev_Option_Init
00000030 T wDev_AppendRxAmpduLensBlocks
0000003c t wDev_SnifferRxLDPC
00000040 b my_event_100
00000040 T wDev_AppendRxBlocks
00000054 t wDev_SnifferRxHT40
00000058 t wDev_ProcessRxSucData
00000094 t wDev_SnifferRxData2
00000110 T wDev_ProcessFiq
00000120 T wDev_Enable_Beacon_Tsf
00000140 T wDev_Disable_Beacon_Tsf
0000015c T wDev_Set_Beacon_Int
00000170 T wDev_Reset_TBTT
000001a0 b WdevRxIsClose_103
000001a4 b BcnSendTick_111
000001a8 B WdevTimOffSet
000001b0 T wDev_Get_Next_TBTT
000001b0 b _wdev_rx_desc_space_115
00000200 b _wdev_rx_mblk_space_116
00000234 t wDev_Rxbuf_Init
000003dc t wDev_AutoAckRate_Init
00000404 t wDev_Bssid_Init
00000474 T wDev_Initialize
000004e8 T wDevForceAck6M
00000510 T wDev_SetMacAddress
000005b4 T wDev_SetRxPolicy
00000610 T wDev_EnableUcRx
00000644 T wDev_DisableUcRx
00000680 T wDev_SetBssid
00000760 T wDev_ClearBssid
00000798 T wDev_Is_Mac_Key_Exist
0000083c T wDev_Insert_KeyEntry
00000974 T wDev_remove_KeyEntry
000009e4 T wDev_remove_KeyEntry_all_cnx
00000a34 T wDev_Crypto_Conf
00000a90 t wDev_Init_dummy_key
00000af0 T wDev_Crypto_Disable
00000b34 T wDevEnableRx
00000b88 T wdev_go_sniffer
00000c68 T wdev_set_sniffer_addr
00000ce0 T wdev_exit_sniffer
000027a0 b _wdev_tx_desc_space_118
00002800 b _wdev_tx_mblk_space_119
00002820 b _wdev_rx_ampdu_len_desc_space_120
00002880 b _wdev_rx_ampdu_len_space_121
00002f80 b MacTimerCb_126
*/

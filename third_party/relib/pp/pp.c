#define MEM_DEFAULT_USE_DRAM 1

#include <assert.h>
#include <stdint.h>
#include "c_types.h"
#include "mem.h"
#include "ets_sys.h"

#include "relib/esp8266.h"
#include "relib/xtensa.h"
#include "relib/s/lldesc.h"
#include "relib/s/esf_buf.h"
#include "relib/s/esf_tx_desc.h"
#include "relib/s/ieee80211com.h"
#include "relib/s/pp_txrx_ctx.h"

#include "relib/ets_rom.h"

#include "lwip/pbuf.h"

static inline bool ICACHE_FLASH_ATTR
pbuf_is_ram_type(struct pbuf *p)
{
#if LIBLWIP_VER == 2
	return pbuf_get_allocsrc(p) == PBUF_TYPE_ALLOC_SRC_MASK_STD_HEAP;
#else
	return p->type == PBUF_RAM;
#endif
}


/* static symbols we forced to global,
 * I think these include line number in their name? */
#define FeedWdtFlag FeedWdtFlag_135
#define pp_soft_wdt_count pp_soft_wdt_count_83
#define pp_sig_cnt pp_sig_cnt_140
#define idle_timer_enabled idle_timer_enabled_124
#define mac_idle_timer mac_idle_timer_125
#define pTxRx pTxRx_150

extern uint8_t FeedWdtFlag;
extern uint8_t dbg_stop_sw_wdt;
extern uint8_t dbg_stop_hw_wdt;
extern uint8_t pp_soft_wdt_count;
extern bool idle_timer_enabled;
extern ETSTimer mac_idle_timer;
extern int sleep_start_wait_time;
extern ieee80211com_st g_ic; /* defined in ieee80211.c */
extern char PendFreeBcnEb;
extern pp_txrx_ctx_st *pTxRx;

enum pptask_sig {
	PPTASK_SIG_TXQ0 = 0,
	PPTASK_SIG_TXQ1 = 1,
	PPTASK_SIG_TXQ2 = 2,
	PPTASK_SIG_TXQ3 = 3,
	PPTASK_SIG_TXDONE = 4,
	PPTASK_SIG_RX =  5,
	PPTASK_SIG_PROCESS_WQ = 8,
	PPTASK_SIG_RX_PKT_HDR = 9,
	PPTASK_SIG_ENABLE_GPIO_WAKEUP = 10,
	PPTASK_SIG_FEED_WDT = 12,
	PPTASK_SIG_WPS_START = 13,
	PPTASK_SIG_TXTIMEOUT = 14,
	PPTASK_SIG_NUM  = 15,
};

extern uint8_t pp_sig_cnt[PPTASK_SIG_NUM];

typedef enum ets_status {
	ETS_OK,
	ETS_FAILED
} ETS_STATUS;

/* called by ppProcTxDone */
esf_buf_st *ppDequeueTxDone_Locked(void);
void pm_incr_active_cnt(void);
void pp_disable_idle_timer(void);
ETS_STATUS ppCheckTxIdle(bool lock);
bool pm_is_waked(void);
bool pm_scan_unlocked(void);
void scan_connect_state(void *ptype);
void tx_pwctrl_background(int a, int b);
void esf_buf_recycle(esf_buf_st *eb, esf_buf_type_t type);

/* called by ppTask */
int ppProcessTxQ(uint8_t ac);
void ppProcTxDone(bool post);
void ppRxPkt(void);
void ppProcessWaitingQueue(void);
typedef void sniffer_buf;
void ppPeocessRxPktHdr(sniffer_buf *sniffer);
void pm_enable_gpio_wakeup(void);
void lmacProcessTxTimeout(void);

void ICACHE_FLASH_ATTR
ppProcTxDone(bool post)
{
	bool active_flag;
	esf_buf_st *eb;
	lldesc_st *lld;
	esf_tx_desc_st *txd;
	uint32_t txd_flags;
	esf_buf_type_t buf_type;

	active_flag = false;
	buf_type = 0;
LAB_40236d36:
	do {
		eb = ppDequeueTxDone_Locked();
		if (eb == NULL) {
			if (active_flag) {
				pm_incr_active_cnt();
			}
			pp_disable_idle_timer();
			ETS_STATUS st = ppCheckTxIdle(true);
			if (st == ETS_OK && pm_is_waked() && pm_scan_unlocked()) {
				idle_timer_enabled = true;
				ets_timer_arm_new(&mac_idle_timer,sleep_start_wait_time,false,true);
			}
			if (g_ic.ic_flags & (1 << 14)) {
				scan_connect_state(g_ic.scaner);
			}
			return;
		}
		uint32_t cb_map = eb->tx_desc->comp_cb_map;
		while(cb_map) {
			/* get mask of lowest set bit */
			uint32_t lowest_set = -cb_map & cb_map;
			/* double-check callback entry is present */
			if (pTxRx->txCBmap & lowest_set) {
				/* get the bit index.
				 * because we know only a single bit
				 * is set, 31 - __builtin_clz is slightly faster than
				 * __builtin_ctz since it maps better to NSAU
				 */
				int bit = 31 - __builtin_clz(lowest_set);
				/* run the callback handler if non-null */
				if (pTxRx->txCBTab[bit] != NULL) {
					pTxRx->txCBTab[bit](eb);
				} else {
					ets_printf("txCBTab[%d] NULL: %08x %08x\n",
						bit, cb_map, lowest_set);
				}
			}
			cb_map ^= lowest_set;
		}
		int pwctrl_thres = 5;
		txd = eb->tx_desc;
		txd_flags = txd->flags;
		if ((txd_flags & (1 << 0x1c)) == 0) {
			static uint8_t need_pwctrl = 0;
			need_pwctrl += txd->rrc; /*((uint8_t)txd->field_0x4 >> 4) */
			if (g_ic.ic_mode == 2) {
				pwctrl_thres = 1;
			}
			if (pwctrl_thres <= need_pwctrl) {
				need_pwctrl = 0;
				tx_pwctrl_background(1,0);
				txd = (eb->desc).tx_desc;
			}
		}
		if ((txd_flags & (1 << 3)) && (!txd->p2p) && txd->status == 1) {
			active_flag = true;
		}
		if (txd_flags & (1 << 0xd)) {
			struct pbuf *pb = (struct pbuf *)eb->pbuf;
			if (pbuf_is_ram_type(pb)) {
				pb->eb = NULL;
			}
			pbuf_free(pb);
			buf_type = ESF_BUF_TX_PB;
			txd_flags = *(uint32_t *)(eb->desc).tx_desc >> 6;
			break;
		}
		if (txd_flags & (1 << 0xe)) {
			buf_type = ESF_BUF_MGMT_LBUF;
			break;
		}
		if (txd_flags & (1 << 0x18)) {
			buf_type = ESF_BUF_MGMT_LLBUF;
			break;
		}
		if (txd_flags & (1 << 0xf)) {
			buf_type = ESF_BUF_MGMT_SBUF;
			break;
		}
	} while (txd_flags & (1 << 0x15));
	if (txd_flags & (1 << 0x17)) {
		eb->data_len -= 4;
		lld = eb->ds_tail;
		lld->length -= 4;
		if (PendFreeBcnEb == 0) goto LAB_40236d36;
		PendFreeBcnEb = 0;
	}
	esf_buf_recycle(eb,buf_type);
	goto LAB_40236d36;
}


void ICACHE_FLASH_ATTR
relib_pptask(ETSEvent *e)
{
	if (e->sig >= PPTASK_SIG_NUM) {
		ets_printf("pptask sig >15: %d!\n", e->sig);
		return;
	}
	enum pptask_sig sig = e->sig;

	uint32_t saved = LOCK_IRQ_SAVE();
	pp_sig_cnt[sig]--;
	LOCK_IRQ_RESTORE(saved);

	if (FeedWdtFlag) {
		if (dbg_stop_sw_wdt == 0) {
			pp_soft_wdt_count = 0;
		}
		if (dbg_stop_hw_wdt == 0) {
			WDT->RST = 0x73;
		}
		FeedWdtFlag = 0;
	}

	switch(sig) {
	case PPTASK_SIG_TXQ0:
	case PPTASK_SIG_TXQ1:
	case PPTASK_SIG_TXQ2:
	case PPTASK_SIG_TXQ3:
		ppProcessTxQ(sig);
		break;
	case PPTASK_SIG_TXDONE:
		ppProcTxDone(true);
		break;
	case PPTASK_SIG_RX:
		ppRxPkt();
		break;
	case PPTASK_SIG_PROCESS_WQ:
		ppProcessWaitingQueue();
		break;
	case PPTASK_SIG_RX_PKT_HDR:
		ppPeocessRxPktHdr((sniffer_buf *)e->par);
		break;
	case PPTASK_SIG_ENABLE_GPIO_WAKEUP:
		pm_enable_gpio_wakeup();
		break;
	case PPTASK_SIG_FEED_WDT:  /* feed wdt, no-op since it got fed above  */
		break;
	case PPTASK_SIG_WPS_START:  /* TODO: WPS-related, not using this right now */
#if 0
		if ((g_ic.ic_wps != NULL && ((g_ic.ic_wps)->wifi_station_wps_start != NULL)) {
			(*(g_ic.ic_wps)->wifi_station_wps_start)();
		}
#endif
		break;
	case PPTASK_SIG_TXTIMEOUT:
		lmacProcessTxTimeout();
		break;
	default:
		ets_printf("unhandled pptask sig: %d\n", sig);
		break;
	}
}

/*
0000005c B AllFreqOffsetInOneChk
00000068 B all_freqoffset_sta_freqcal
         U ap_freq_force_to_scan
00000060 B AvgFreqOffsetInOneChk
0000006a B avg_freqoffset_sta_freqcal
         U BcnWithMcastSendCnt
00000210 B buffed_eb_arr
         U Cache_Read_Enable_New
00000014 D CanDoFreqCal
00000f40 t CheckBcnMeetReq
         U chip6_phy_init_ctrl
         U chip_v6_set_chan_offset
         U chm_get_current_channel
0000003e b cur_idx_168
00000351 b current_ifidx$9784_95_1
0000003b B dbg_stop_hw_wdt
0000003a B dbg_stop_sw_wdt
00000070 B DefFreqCalTimer
00001384 T DefFreqCalTimerCB
         U __divsi3
         U esf_buf_recycle
         U esf_buf_setup
         U ets_intr_lock
         U ets_intr_unlock
         U ets_memcmp
         U ets_post
         U ets_printf
         U ets_task
         U ets_timer_arm_new
         U ets_timer_disarm
         U ets_timer_setfn
00000039 b FeedWdtFlag_135
         U flashchip
00000010 t flash_str$7335_75_3
00000020 t flash_str$7336_75_4
00000030 t flash_str$7689_99_5
00000040 t flash_str$8355_138_4
00000060 t flash_str$8472_143_12
         U fpm_allow_tx
         U fpm_do_wakeup
         U fpm_rf_is_closed
00001184 T freq_change_check_scan_done
000010fc T freq_change_check_scan_start
00001344 T freq_change_check_scan_work
00000040 B freq_change_sta_scan
00001030 T freq_change_sta_scan_force_enable
00001050 T freq_changle_sta_scan_do_cb
         U GetAccess
         U g_ic
         U gScanStruct
000007cc T HdlAllBuffedEb
000013fc t HdlChlFreqCal
00000062 B HighestFreqOffsetInOneChk
         U hostap_input
         U ic_get_gtk_alg
         U ic_get_ptk_alg
         U ic_is_pure_sta
00000021 b idle_timer_enabled_124
00000038 B idle_timer_reopen_flag
0000003c b idx_to_be_use_153
00000000 t ieee80211_hdrsize
         U lldesc_num2link
         U lmacDiscardAgedMSDU
         U lmacIsIdle
         U lmacMSDUAged
         U lmacProcessTxTimeout
         U lmacTxFrame
00000064 B LowestFreqOffsetInOneChk
00000024 b mac_idle_timer_125
00000000 t mem_debug_file_132
00000350 b need_pwctrl$7949_92_7
00000019 b node_check_tick_111
         U noise_check_loop
00000006 d noise_now_114
00000004 b noise_test_timer_105
00000004 D NoiseTimerInterval
         U os_printf_plus
         U pbuf_free
00000018 B pend_flag_noise_check
00000020 B pend_flag_periodic_cal
         U PendFreeBcnEb
00000008 d PeriodCalInterval_116
0000001c b period_cal_tick_115
         U periodic_cal_top
00000084 B PktNumInOneChk
00000066 B pktnum_sta_freqcal
00000058 B PktsNumInOneChk
         U pm_allow_tx
         U pm_assoc_parse
0000006c B pmdofreqcal
         U pm_enable_gpio_wakeup
         U pm_incr_active_cnt
         U pm_is_open
         U pm_is_waked
         U pm_onBcnRx
         U pm_post
         U pm_rf_is_closed
         U pm_scan_unlocked
         U pm_set_addr
         U pm_sleep_for
000019f0 T pp_attach
00000010 T ppCalFrameTimes
00000024 T ppCalTxop
00000b54 T ppCheckTxIdle
00001970 t ppDequeueRxq_Locked
00001928 t ppDequeueTxDone_Locked
00000004 T ppDequeueTxQ
0000034c T pp_disable_idle_timer
0000015c T pp_disable_noise_timer
00000008 T ppDiscardMPDU
0000037c T pp_enable_idle_timer
000001cc T pp_enable_noise_timer
00000004 T ppEnqueueRxq
00000014 T ppEnqueueTxDone
00000004 T ppFetchTxQFirstAvail
00000014 t ppGetTxframe
000018cc T ppGetTxQFirstAvail_Locked
00001ba8 t ppMapTxQueue
00000030 t ppMapWaitTxq
000002e8 T pp_noise_test
000003f4 T ppPeocessRxPktHdr
00000028 T pp_post
00000008 T pp_post2
00000020 T ppProcessTxQ
00001af4 t ppProcessWaitingQueue
00000a64 T ppProcessWaitQ
00000c74 t ppProcTxDone
00001d78 t ppProcTxSecFrame
00000004 T ppRecordBarRRC
00000ae0 T ppRecycleRxPkt
00000a78 T ppRegisterTxCallback
00000004 T ppRollBackTxQ
00000e78 t ppRxPkt
000016a0 t ppRxProtoProc
00000020 t ppSearchTxframe
00000004 t ppSearchTxQueue
0000000c t ppSelectNextQueue
00000200 b pp_sig_cnt_140
00000000 b pp_soft_wdt_count_83
00000074 T pp_soft_wdt_feed
00000034 T pp_soft_wdt_feed_local
0000004c T pp_soft_wdt_init
000000e0 T pp_soft_wdt_restart
0000009c T pp_soft_wdt_stop
000004f0 t ppTask
00000240 b ppTaskQ_265
000003b4 T pp_try_enable_idle_timer
00000e40 T pp_tx_idle_timeout
000008ec T ppTxPkt
000006f8 t ppTxProtoProc
00000004 T ppTxqUpdateBitmap
00000aa4 T ppUnregisterTxCallback
00000010 T PPWdtReset
         U promiscuous_cb
00000010 d pTxRx_150
         U RC_GetAckTime
         U RC_GetBlockAckTime
         U RC_GetCtsTime
         U rcGetSched
         U rc_get_trc
         U rcUpdateRxDone
         U read_hw_noisefloor
0000030c T reset_noise_timer
00000118 T RxNodeNum
         U scan_connect_state
         U scan_hidden_ssid
         U scan_start
0000000c D sleep_start_wait_time
00000000 D soft_wdt_interval
         U sta_input
00000000 r __switch_array_310
00000004 r __switch_array_328
0000054f t __switchjump_table_xs_89_10
         U system_restart_local
         U system_rtc_mem_write
00000074 T system_soft_wdt_feed
000000e0 T system_soft_wdt_restart
0000009c T system_soft_wdt_stop
00000018 D tcb
00000015 D test_freq_val_first
00000054 B test_freq_val_force_flag
         U TestStaFreqCalValDev
00000056 B TestStaFreqCalValFilter
         U TestStaFreqCalValInput
         U TestStaFreqCalValOK
         U TmpSTAAPCloseAP
0000003d B total_buffed_eb_num
00000134 T TxNodeNum
         U tx_pwctrl_background
00000088 b TxRxCxt_293
         U __udivsi3
         U vPortFree
         U Wait_SPI_Idle
         U wDev_AppendRxBlocks
         U wDevCtrl
         U wDev_MacTim1Arm
         U wDev_MacTim1SetFunc
*/

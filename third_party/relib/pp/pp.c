#define MEM_DEFAULT_USE_DRAM 1

#include <assert.h>
#include <stdint.h>
#include "c_types.h"
#include "mem.h"
#include "ets_sys.h"

#include "relib/esp8266.h"
#include "relib/xtensa.h"

#include "relib/ets_rom.h"

#include "lwip/pbuf.h"

/* static symbols we forced to global,
 * I think these include line number in their name? */
#define FeedWdtFlag FeedWdtFlag_135
#define pp_soft_wdt_count pp_soft_wdt_count_83
#define pp_sig_cnt pp_sig_cnt_140

extern uint8_t FeedWdtFlag;
extern uint8_t dbg_stop_sw_wdt;
extern uint8_t dbg_stop_hw_wdt;
extern uint8_t pp_soft_wdt_count;

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

int ppProcessTxQ(uint8_t ac);
void ppProcTxDone(bool post);
void ppRxPkt(void);
void ppProcessWaitingQueue(void);
typedef void sniffer_buf;
void ppPeocessRxPktHdr(sniffer_buf *sniffer);
void pm_enable_gpio_wakeup(void);
void lmacProcessTxTimeout(void);

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

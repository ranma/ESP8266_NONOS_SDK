#define MEM_DEFAULT_USE_DRAM 1

#include <assert.h>
#include <stdint.h>
#include "c_types.h"
#include "mem.h"

#include "relib/esp8266.h"
#include "relib/xtensa.h"

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

extern uint8_t *pp_sig_cnt;

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
	ETSSignal sig;
	uint32_t saved;

	if (FeedWdtFlag == '\x01') {
		saved = LOCK_IRQ_SAVE();
		if (dbg_stop_sw_wdt == '\0') {
			pp_soft_wdt_count = '\0';
		}
		if (dbg_stop_hw_wdt == '\0') {
			WDT->RST = 0x73;
		}
		FeedWdtFlag = '\0';
		LOCK_IRQ_RESTORE(saved);
	}
	sig = e->sig;
	switch(sig) {
	case 0:
	case 1:
	case 2:
	case 3:
		pp_sig_cnt[sig] = pp_sig_cnt[sig] + 0xff;
		ppProcessTxQ((uint8_t)sig);
		break;
	case 4:
		saved = LOCK_IRQ_SAVE();
		pp_sig_cnt[e->sig] = pp_sig_cnt[e->sig] + 0xff;
		LOCK_IRQ_RESTORE(saved);
		ppProcTxDone(true);
		break;
	case 5:
		saved = LOCK_IRQ_SAVE();
		pp_sig_cnt[e->sig] = pp_sig_cnt[e->sig] + 0xff;
		LOCK_IRQ_RESTORE(saved);
		ppRxPkt();
		break;
	case 6:
		break;
	case 7:
		break;
	case 8:
		pp_sig_cnt[sig] = pp_sig_cnt[sig] + 0xff;
		ppProcessWaitingQueue();
		break;
	case 9:
		ppPeocessRxPktHdr((sniffer_buf *)e->par);
		break;
	case 10:
		saved = LOCK_IRQ_SAVE();
		pp_sig_cnt[e->sig] = pp_sig_cnt[e->sig] + 0xff;
		LOCK_IRQ_RESTORE(saved);
		pm_enable_gpio_wakeup();
		break;
	case 0xb:
		saved = LOCK_IRQ_SAVE();
		pp_sig_cnt[e->sig] = pp_sig_cnt[e->sig] + 0xff;
		LOCK_IRQ_RESTORE(saved);
		break;
	case 0xc:
		saved = LOCK_IRQ_SAVE();
		pp_sig_cnt[e->sig] = pp_sig_cnt[e->sig] + 0xff;
		/* sic: This got cleared before the switch, unreachable code? */
		if (FeedWdtFlag == '\x01') {
			if (dbg_stop_sw_wdt == '\0') {
				pp_soft_wdt_count = '\0';
			}
			if (dbg_stop_hw_wdt == '\0') {
				WDT->RST = 0x73;
			}
			FeedWdtFlag = '\0';
		}
		LOCK_IRQ_RESTORE(saved);
		break;
	case 0xd:  /* TODO: WPS-related, not using this right now */
		saved = LOCK_IRQ_SAVE();
		pp_sig_cnt[e->sig] = pp_sig_cnt[e->sig] + 0xff;
		LOCK_IRQ_RESTORE(saved);
#if 0
		if ((g_ic.ic_wps != NULL && ((g_ic.ic_wps)->wifi_station_wps_start != NULL)) {
			(*(g_ic.ic_wps)->wifi_station_wps_start)();
		}
#endif
		break;
	case 0xe:
		saved = LOCK_IRQ_SAVE();
		pp_sig_cnt[e->sig] = pp_sig_cnt[e->sig] + 0xff;
		LOCK_IRQ_RESTORE(saved);
		lmacProcessTxTimeout();
		break;
	}
}

#include <assert.h>
#include <stdint.h>

#include "c_types.h"
#include "mem.h"
#include "osapi.h"
#include "spi_flash.h"
#include "gpio.h"

#include "relib/relib.h"
#include "relib/esp8266.h"
#include "relib/xtensa.h"
#include "relib/ets_rom.h"

#include "relib/s/chm_ctx.h"
#include "relib/s/cnx_mgr.h"
#include "relib/s/ieee80211com.h"
#include "relib/s/ieee80211_channel.h"

#if 1
#define DPRINTF(fmt, ...) do { \
	ets_printf("[@%p] " fmt, __builtin_return_address(0), ##__VA_ARGS__); \
	while ((UART0->STATUS & 0xff0000) != 0); \
} while (0)
#else
#define DPRINTF(fmt, ...)
#endif

extern ieee80211com_st g_ic; /* ieee80211.c */
extern bool BcnEb_update;

static chm_ctx_st gChmCtx;

ieee80211_channel_st* ICACHE_FLASH_ATTR
chm_get_current_channel()
{
	return gChmCtx.current_channel;
}

STATUS phy_change_channel(int freq, int unused_arg2, int unused_arg3);
int16_t ieee80211_chan2ieee(ieee80211_channel_st *c);

void ICACHE_FLASH_ATTR
chm_set_current_channel(ieee80211_channel_st *chan)
{
	phy_change_channel(chan->ch_freq,1,0);
	uint32_t saved = LOCK_IRQ_SAVE();
	gChmCtx.current_channel = chan;
	LOCK_IRQ_RESTORE(saved);
}

bool ICACHE_FLASH_ATTR
chm_check_same_channel(void)
{
	int home_ch = ieee80211_chan2ieee(g_ic.ic_home_channel);
	int current_ch = ieee80211_chan2ieee(chm_get_current_channel());
	return home_ch == current_ch;
}

void ICACHE_FLASH_ATTR
chm_end_op(STATUS status)
{
	chm_cb_t cb = gChmCtx.op.end_cb;
	void *cb_arg = gChmCtx.op.arg;
	bzero(&gChmCtx.op,sizeof(gChmCtx.op));
	if (cb != NULL) {
		/* Bug: original firmware is using arg after bzero */
		cb(cb_arg,status);
	}
}


void ICACHE_FLASH_ATTR
chm_cancel_op(void)
{
	if (gChmCtx.op.channel != NULL) {
		ets_timer_disarm(&gChmCtx.chm_min_dwell_timer);
		ets_timer_disarm(&gChmCtx.chm_max_dwell_timer);
		chm_end_op(CANCEL);
	}
	if (gChmCtx.op_cb != NULL) {
		gChmCtx.op_cb(gChmCtx.op_cb_arg,CANCEL);
	}
}

void ICACHE_FLASH_ATTR
chm_change_channel(void)
{
	chm_cb_t start_fn = gChmCtx.op.start_fn;

	ieee80211_channel_st *ch = chm_get_current_channel();
	if (ch->ch_freq != (gChmCtx.op.channel)->ch_freq) {
		DPRINTF("chm_change_channel(%d -> %d)\n",
			ieee80211_chan2ieee(ch),
			ieee80211_chan2ieee(gChmCtx.op.channel));
		chm_set_current_channel(gChmCtx.op.channel);
	}
	if (start_fn != NULL) {
		start_fn(gChmCtx.op.arg, OK);
	}
	if (gChmCtx.op.min_duration == 0) {
		if (gChmCtx.op.max_duration == 0) {
			bzero(&gChmCtx.op,sizeof(gChmCtx.op));
			return;
		}
	}
	else if (gChmCtx.op.min_duration < gChmCtx.op.max_duration) {
		ets_timer_disarm(&gChmCtx.chm_min_dwell_timer);
		ets_timer_arm_new(&gChmCtx.chm_min_dwell_timer,gChmCtx.op.min_duration,false /*repeat_flag*/,true /*ms_flag*/);
	}
	ets_timer_disarm(&gChmCtx.chm_max_dwell_timer);
	ets_timer_arm_new(&gChmCtx.chm_max_dwell_timer,gChmCtx.op.max_duration,false /*repeat_flag*/,true /*ms_flag*/);
}


STATUS ICACHE_FLASH_ATTR
chm_start_op(ieee80211_channel_st *ch, uint32_t min_duration,uint32_t max_duration, chm_cb_t start_fn,chm_cb_t end_fn,void *arg)
{
	DPRINTF("chm_start_op(ch=%d, min_duration=%d, max_duration=%d\n",
		ieee80211_chan2ieee(ch), min_duration, max_duration);
	if (gChmCtx.op.channel != NULL) {
		return BUSY;
	}

	gChmCtx.op.channel = ch;
	gChmCtx.op.min_duration = min_duration;
	gChmCtx.op.max_duration = max_duration;
	gChmCtx.op.arg = arg;
	gChmCtx.op.start_fn = start_fn;
	gChmCtx.op.end_cb = end_fn;
	if ((g_ic.cnxmgr->cnx_state & CNX_S_DISCONNECTED) == 0) {
		chm_change_channel();
	}
	else {
		chm_change_channel();
	}
	return OK;
}


STATUS ICACHE_FLASH_ATTR
chm_acquire_lock(uint8 priority, chm_cb_t cb,void *arg)
{
	if (gChmCtx.op_lock) {
		if (gChmCtx.op_priority < priority) {
			return BUSY;
		}
		chm_cancel_op();
	}
	gChmCtx.op_priority = priority;
	gChmCtx.op_lock = true;
	gChmCtx.op_cb_arg = arg;
	gChmCtx.op_cb = cb;
	return OK;
}

void ICACHE_FLASH_ATTR
chm_release_lock(void)
{
	gChmCtx.op_cb_arg = NULL;
	gChmCtx.op_cb = NULL;
	gChmCtx.op_lock = false;
	gChmCtx.op_priority = 0;
}

void ICACHE_FLASH_ATTR
chm_return_home_channel(void)
{
	ieee80211_channel_st *home_ch = g_ic.ic_home_channel;
	ieee80211_channel_st *current_ch = chm_get_current_channel();
	if (home_ch->ch_freq != current_ch->ch_freq) {
		chm_set_current_channel(home_ch);
	}
}

void ICACHE_FLASH_ATTR
chm_end_op_timeout(void *unused_arg)
{
	chm_end_op(OK);
}

void ICACHE_FLASH_ATTR
chm_init(ieee80211com_st *ic)
{
	int chIdx = 0;

	gChmCtx.ic = ic;

	if (g_ic.ic_profile.opmode != STATION_MODE) {
		chIdx = g_ic.ic_profile.softap.channel - 1;
	}

	uint32_t saved = LOCK_IRQ_SAVE();
	ieee80211_channel_st *chan = &ic->ic_channels[chIdx];
	if (ic->ic_home_channel != chan) {
		BcnEb_update = true;
	}
	ic->ic_home_channel = chan;
	LOCK_IRQ_RESTORE(saved);

	chm_set_current_channel(chan);
	ets_timer_setfn(&gChmCtx.chm_min_dwell_timer,chm_end_op_timeout,(void *)0);
	ets_timer_setfn(&gChmCtx.chm_max_dwell_timer,chm_end_op_timeout,(void *)1);
}


/*
         U BcnEb_update
         U ets_bzero
         U ets_intr_lock
         U ets_intr_unlock
         U ets_timer_arm_new
         U ets_timer_disarm
         U ets_timer_setfn
         U g_ic
         U ieee80211_chan2ieee
         U phy_change_channel
00000000 b gChmCtx_72
00000004 t chm_end_op_timeout
00000004 T chm_freq2index
00000004 T chm_get_current_channel
00000004 T chm_release_lock
0000000c T chm_acquire_lock
0000000c T chm_end_op
0000000c T chm_return_home_channel
00000010 T chm_check_same_channel
00000010 T chm_set_current_channel
00000010 T chm_start_op
00000018 T chm_cancel_op
0000002c t chm_change_channel
0000002c T chm_init
*/

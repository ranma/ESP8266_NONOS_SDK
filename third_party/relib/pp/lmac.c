#include <assert.h>
#include "c_types.h"
#include "osapi.h"
#include "mem.h"

#include "relib/esp8266.h"
#include "relib/xtensa.h"
#include "relib/ets_rom.h"
#include "relib/ets_timer.h"
#include "relib/relib.h"
#include "relib/s/access.h"
#include "relib/s/esf_buf.h"

#if 1
#define maybe_extern extern
#else
#define maybe_extern
#endif

#define our_active_index our_active_index_90
#define our_instances our_instances_92
#define our_wait_eb our_wait_eb_100
#define our_wait_clear_type our_wait_clear_type_123

maybe_extern access_st our_instances[8];
maybe_extern esf_buf_st *our_wait_eb;
maybe_extern uint8_t our_wait_clear_type;
maybe_extern uint8_t our_active_index;

void
lmacTryTxopEnd(access_st *my, bool success)
{
	if (my->trytxop != false) {
		my->intxop = success;
		my->trytxop = false;
	}
}

bool
lmacIsActive(void)
{
	return our_active_index < 8;
}

bool
lmacIsIdle(uint8_t ac)
{
	return our_instances[ac].state == 0;
}

uint8_t wDev_SetFrameAckType(uint8_t type,uint8_t ack_type);
void wDev_SetWaitingQueue(uint8_t index);
void wDev_ClearWaitingQueue(uint8_t index);

void
lmacSetWaitQueue(access_st *my, uint8_t type)
{
	if (our_wait_eb == NULL) {
		ets_printf("%s %u\n","mac",0xed);
		while (1);
	}
	my->org_acktype = wDev_SetFrameAckType(type, 3);
	wDev_SetWaitingQueue('\n');
}

void
lmacClearWaitQueue(access_st *my, uint8_t type)
{
	if (our_wait_eb == NULL) {
		ets_printf("%s %u\n","mac",0xf8);
		while(1);
	}
	wDev_SetFrameAckType(type, my->org_acktype);
	wDev_ClearWaitingQueue('\n');
}

void lmacSetTxFrame(access_st *my, bool use_wait_queue);

void
lmacPrepareImrTxFrame(access_st *my, bool use_wait_queue)
{
	lmacSetTxFrame(my,use_wait_queue);
}

void lmacProcessCollisions(void);
void lmacContinueFrameExchangeSequence(access_st *my);

void
lmacProcessTXStartData(uint8_t index)
{
	int selected_index;
	int active_index;
	
	active_index = our_active_index;
	selected_index = index;
	if (index == 10) {
		selected_index = active_index;
	}
	if (7 < selected_index) {
		ets_printf("%s %u\n", "mac",0x23b);
		while (1);
	}
	if (our_instances[selected_index].state != 1) {
		if (7 < active_index) {
			return;
		}
		selected_index = active_index;
		if (our_instances[active_index].state != 1) {
			ets_printf("%s %u\n", "mac",0x247);
			while (1);
		}
	}

	access_st *my = &our_instances[selected_index];
	our_active_index = selected_index;
	if (index == 10) {
		lmacClearWaitQueue(my, our_wait_clear_type);
		our_wait_eb = NULL;
	}
	if (my->tx_frame == NULL) {
		ets_printf("%s %u\n", "mac", 0x24f);
		while (1);
	}

	my->state = 2;
	lmacContinueFrameExchangeSequence(my);
	lmacProcessCollisions();
}


access_st*
GetAccess(uint8_t ac)
{
	return &our_instances[ac];
}



/*
         U ets_delay_us
         U ets_intr_lock
         U ets_intr_unlock
         U ets_post
         U ets_printf
         U os_printf_plus
         U ppCalFrameTimes
         U ppCalTxop
         U ppDequeueTxQ
         U ppEnqueueRxq
         U ppEnqueueTxDone
         U ppFetchTxQFirstAvail
         U ppGetTxQFirstAvail_Locked
         U pp_post
         U ppRecordBarRRC
         U ppRollBackTxQ
         U ppTxqUpdateBitmap
         U rcAttach
         U RC_GetAckTime
         U rcGetRate
         U rcReachRetryLimit
         U RC_SetBasicRate
         U rcUpdateTxDone
         U Tx_Copy2Queue
         U wDev_ClearTxqCollisions
         U wDev_ClearWaitingQueue
         U wDev_EnableTransmit
         U wDev_GetBAInfo
         U wDev_GetTxqCollisions
         U wDev_ProcessCollision
         U wDev_SetFrameAckType
         U wDev_SetWaitingQueue
00000000 t flash_str$7920_132_5
00000000 t lmacTryTxopEnd
00000000 b our_active_index_90
00000004 T GetAccess
00000004 B lmacConfMib
00000004 T lmacDiscardAgedMSDU
00000004 T lmacInitAc
00000004 T lmacIsActive
00000004 T lmacIsIdle
00000004 t lmacPrepareImrTxFrame
00000004 T lmacProcessAllTxTimeout
00000004 T lmacProcessRtsStart
00000004 T lmacRecycleMPDU
00000008 t lmacEndFailTxop
00000008 t lmacImrTxFrame
00000008 T lmacMSDUAged
00000008 T lmacRxDone
0000000c t lmacDiscardMSDU
0000000c t lmacEndFailFrag
0000000c t lmacEndFailNoStart
0000000c T lmacProcessTxTimeout
0000000c T lmacSetAcParam
00000010 T lmacProcessCollisions
00000014 T lmacProcessCtsTimeout
00000014 t lmacTxDone
00000018 t lmacClearWaitQueue
00000018 t lmacSetWaitQueue
00000020 t flash_str$7951_133_4
00000020 T lmacProcessAckTimeout
00000024 t lmacDiscardFrameExchangeSequence
00000024 T lmacProcessCollision
00000024 b our_wait_eb_100
00000028 B our_tx_eb
0000002c t lmacRetryTxFrame
0000002c b our_wait_clear_type_123
00000030 b our_instances_92
00000034 T lmacProcessTxError
00000038 t lmacProcessLongRetryFail
00000038 T lmacProcessTxSuccess
0000003c t lmacDisableTransmit
0000003c T lmacProcessTXStartData
00000040 T lmacInit
00000044 t lmacContinueFrameExchangeSequence
00000044 T lmacProcessTxRtsError
00000044 T lmacTxFrame
0000004c t lmacProcessShortRetryFail
00000050 t lmacSetTxFrame
000000c4 t lmacMibInit
000000c8 t lmacEndFrameExchangeSequence
000000fc T lmacSetRetryLimit
*/

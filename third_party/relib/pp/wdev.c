#include <assert.h>
#include "c_types.h"
#include "osapi.h"

#include "relib/esp8266.h"
#include "relib/xtensa.h"
#include "relib/ets_rom.h"
#include "relib/ets_timer.h"
#include "relib/relib.h"
#include "relib/s/lldesc.h"
#include "relib/s/esf_buf.h"
#include "relib/s/wdev_control.h"
#include "relib/s/wdev_status.h"

#define my_event my_event_100

extern wdev_control_st wDevCtrl;
extern wdev_status_st my_event;
extern esf_buf_st *our_tx_eb;

void wDev_MacTimerISRHdl(char idx);
void wDev_SnifferRxLDPC(void);
void wDev_SnifferRxHT40(void);
void wDev_ProcessRxSucData(lldesc_st * tail, uint16_t nblks, uint32_t rxstart_time, uint8_t rxtime_valid);
void wDev_DiscardFrame(lldesc_st * tail, uint16_t nblks);
void wDev_SnifferRxData2(lldesc_st * head);
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

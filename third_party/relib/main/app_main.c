#include <assert.h>
#include <stdint.h>

#include "c_types.h"
#include "mem.h"
#include "osapi.h"
#include "spi_flash.h"

#include "relib/relib.h"
#include "relib/esp8266.h"
#include "relib/xtensa.h"
#include "relib/ets_rom.h"
#include "relib/s/if_param.h"
#include "relib/s/rst_info.h"
#include "relib/s/ieee80211com.h"

typedef void (*nmi_timer_cb_t)(void);

extern ieee80211com_st g_ic; /* defined in ieee80211.c */
extern int system_param_sector_start;

if_param_st info;
uint16_t lwip_timer_interval;
ETSTimer check_timeouts_timer;
uint32_t system_phy_init_sector;
uint32_t system_rf_cal_sector;
bool freq_trace_enable;
bool user_init_flag;
nmi_timer_cb_t NmiTimerCb;

const char SDK_VERSION[] = "3.0.5";

uint32_t system_get_time(void);
void pp_soft_wdt_init(void);
void wifi_softap_set_default_ssid(void);
void wifi_station_set_default_hostname(uint8_t *mac);
void wDev_Set_Beacon_Int(uint32_t beacon_int);
bool system_param_save_with_protect(uint16_t start_sec, void *param, uint16_t len);


bool __attribute__((weak))
user_iram_memory_is_enabled(void)
{
	return 0;
}

void
NMI_Handler(void)
{
	do {
		DPORT->NMI_INT_ENABLE = (DPORT->NMI_INT_ENABLE & 0xffffffe0) | 0xe;
	} while (DPORT->NMI_INT_ENABLE & 1);

	if (NmiTimerCb != NULL) {
		NmiTimerCb();
	}

	INTCLEAR(8);
	FRC1->INT &= ~1;
}

void ICACHE_FLASH_ATTR
user_uart_wait_tx_fifo_empty(uint8_t uart_no, uint32_t time_out_us)
{
	uint32_t uVar1;
	uint32_t uVar2;
	uint32_t uVar3;

	uint32_t timeout = WDEV->MICROS + time_out_us;
	volatile uint32_t *status_reg = uart_no ? &UART1->STATUS : &UART0->STATUS;

	while ((*status_reg & 0xff0000) != 0 && TIME_AFTER(timeout, WDEV->MICROS));
}

STATUS user_uart_tx_one_char(uint8 TxChar)
{
	uint32_t deadline = WDEV->MICROS + 100000;
	do {
		if ((UART0->STATUS & 0xff0000) >> 0x10 < 0x7e) {
			UART0->FIFO = TxChar;
			return OK;
		}
	} while (TIME_AFTER(deadline, WDEV->MICROS));
	return OK;
}


void
user_uart_write_char(char c)
{
	if (c == '\n') {
		user_uart_tx_one_char('\r');
		user_uart_tx_one_char('\n');
	}
	else if (c != '\r') {
		user_uart_tx_one_char(c);
	}
}


void ICACHE_FLASH_ATTR
uart_div_modify(uint8_t uart_no, uint32_t DivLatchValue)
{
	struct uart_regs *uart = uart_no ? UART1 : UART0;
	/* ROM implementation doesn't include the mask here, but
	 * effectively both are problematic when the value is out of range */
	uart->CLKDIV = DivLatchValue & 0xfffff;
	uart->CONF0 |=  0x00060000;
	uart->CONF0 &= ~0x00060000;
}

uint8_t ICACHE_FLASH_ATTR
esp_crc8(uint8_t *data, int len)
{
	/* Dallas/Maxim-style CRC8, with init value 0 and poly 0x31 * (X^8+X^5+X^4+X^0) */
	uint32_t crc = 0;

	while (len-- > 0) {
		crc ^= *data++;
#if 0
		/* straight-forward looping impl */
		for (int i = 0; i < 8; i++) {
			if (crc & 0x80) {
				crc = (crc << 1) ^ 0x31;
			} else {
				crc <<= 1;
			}
		};
#else
		/* unrolled impl */
		uint32_t tmp = crc << 3;
		crc ^= tmp;
		tmp <<= 1;
		crc ^= tmp;
		tmp <<= 2;
		crc = crc ^ tmp;

		crc &= 0xff;

		tmp = crc >> 4;
		crc ^= tmp;
		tmp >>= 1;
		crc ^= tmp;
#endif
	}
	return crc;
}

int ICACHE_FLASH_ATTR
read_macaddr_from_otp(uint8_t *mac)
{
	uint32_t otp0 = EFUSE->DATA[0];
	uint32_t otp1 = EFUSE->DATA[1];
	uint32_t otp2 = EFUSE->DATA[2];
	uint32_t otp3 = EFUSE->DATA[3];
	
	if (((otp2 >> 0xf) & 1) == 0) {
		os_printf("Firmware ONLY supports ESP8266!!!\n");
		while (1);
	}
	if (otp0 == 0 && otp1 == 0) {
		os_printf("empty otp\n");
		while (1);
	}
	mac[3] = otp1 >> 8;
	mac[4] = otp1;
	mac[5] = otp0 >> 24;
	if (!(otp2 & EFUSE_D2_IS_48BITS_MAC)) {
		mac[0] = 0x18;
		mac[1] = 0xfe;
		mac[2] = 0x34;
	}
	else {
		uint8_t rev_mac[4];
		uint8_t efuse_crc = otp2 >> 24;

		mac[0] = otp3 >> 16;
		mac[1] = otp3 >> 8;
		mac[2] = otp3;

		rev_mac[0] = mac[2];
		rev_mac[1] = mac[1];
		rev_mac[2] = mac[0];
		uint8_t crc = esp_crc8(rev_mac,3);
		if (crc != efuse_crc) {
			os_printf_plus("Mh\n");
			return 1;
		}
		os_printf_plus("V%d\n", otp2);
		if (((otp1 >> EFUSE_D1_VERSION_SHIFT) & 3) > 0) {
			rev_mac[0] = mac[5];
			rev_mac[1] = mac[4];
			rev_mac[2] = mac[3];
			rev_mac[3] = otp1 >> 16;
			efuse_crc = otp0 >> 16;
			crc = esp_crc8(rev_mac,4);
			if (crc != efuse_crc) {
				os_printf_plus("Ml\n");
				return 1;
			}
		}
	}
	os_printf_plus("Mo\n");
	return 0;
}

void
wdt_feed(void *unused_arg)
{
#if 0
	/* Original code fills in rst_info, only to overwrite it using system_rtc_mem_read... */
	rst_info_st rst_info;
	rst_info.exccause = *in_EXCCAUSE;
	rst_info.epc1 = *in_EPC1;
	rst_info.epc2 = *in_EPC2;
	rst_info.epc3 = *in_EPC3;
	rst_info.excvaddr = *in_EXCVADDR;
	rst_info.depc = *in_DEPC;
	system_rtc_mem_read(0, &rst_info, 0x1c);
	RTC->STORE[0] = REASON_WDT_RST;
	rst_info.reason = REASON_WDT_RST;
	system_rtc_mem_write(0, &rst_info, 0x1c);
#else
	rst_info_st *rst_info = (rst_info_st*)&RTCMEM->SYSTEM[0];
	RTC->STORE[0] = REASON_WDT_RST;
	rst_info->reason = REASON_WDT_RST;
#endif
}

void ICACHE_FLASH_ATTR __attribute__((weak))
wifi_set_backup_mac(const uint8_t *src)
{
}

int ICACHE_FLASH_ATTR
flash_data_check(uint8_t *data)
{
	uint32_t sum = 0;
	for (int i = 0; i < 24; i++, data+= 4) {
		sum += data[0] | (data[1]<<8) | (data[2]<<16) | (data[3]<<24);
	}
	sum += ((EFUSE->DATA[2] & 0xF000)<<16) | (EFUSE->DATA[1] & 0xfffffff);
	sum += (EFUSE->DATA[0] & 0xff000000) | (EFUSE->DATA[3] & 0xffffff);
	sum ^= 0xFFFFFFFF;

	uint32_t csum = data[8] | (data[9]<<8) | (data[10]<<16) | (data[11]<<24);
	if (sum != csum) {
		return 1;
	}
	return 0;
}

void ICACHE_FLASH_ATTR
wdt_init(bool enable)
{
	if (enable) {
		WDT->CTL &= ~1;
		ets_isr_attach(8, wdt_feed, 0);
		DPORT->EDGE_INT_ENABLE |= 1;
		WDT->OP = 0xb;
		WDT->OP_ND = 0xd;
		WDT->CTL |= 0x38;
		WDT->CTL &= ~6;
		WDT->CTL |= 1;
		ets_isr_unmask(0x100);
	}
	pp_soft_wdt_init();
}

void ICACHE_FLASH_ATTR
user_init_default_profile(void)
{
	bool bVar1;
	uint32_t uVar2;
	
	bVar1 = g_ic.ic_profile.opmode == 0xff;
	if (bVar1) {
		g_ic.ic_profile.opmode = '\x02';
	}
	wifi_softap_set_default_ssid();
	wifi_station_set_default_hostname((char *)info.sta_mac);
	if ((0xd < g_ic.ic_profile.softap.channel) || (g_ic.ic_profile.softap.channel == '\0')) {
		g_ic.ic_profile.softap.channel = '\x01';
	}
	if ((60000 < g_ic.ic_profile.softap_beacon_interval) ||
		 (g_ic.ic_profile.softap_beacon_interval < 100)) {
		g_ic.ic_profile.softap_beacon_interval = 100;
	}
	uVar2 = g_ic.ic_profile.softap_beacon_interval / 100;
	wDev_Set_Beacon_Int((uVar2 & 0xffff) * 0x19000);
	if ((4 < g_ic.ic_profile.softap.authmode) || (g_ic.ic_profile.softap.authmode == '\x01')) {
		g_ic.ic_profile.softap.authmode = '\0';
		bzero(g_ic.ic_profile.softap.password,0x40);
	}
	if (1 < g_ic.ic_profile.softap.ssid_hidden) {
		g_ic.ic_profile.softap.ssid_hidden = '\0';
	}
	if (8 < g_ic.ic_profile.softap.max_connection) {
		g_ic.ic_profile.softap.max_connection = '\x04';
	}
	if (g_ic.ic_profile.sta.ssid.len == -1) {
		bzero(&g_ic.ic_profile.sta,0x24);
		bzero(g_ic.ic_profile.sta.password,0x40);
	}
	g_ic.ic_profile.wps_status = WPS_STATUS_DISABLE;
	g_ic.ic_profile.wps_type = WPS_TYPE_DISABLE;
	g_ic.ic_profile.led.open_flag = '\0';
	if (true < g_ic.ic_profile.sta.bssid_set) {
		g_ic.ic_profile.sta.bssid_set = false;
	}
	if (5 < g_ic.ic_profile.ap_change_param.ap_number) {
		g_ic.ic_profile.ap_change_param.ap_number = '\x01';
	}
	if ((IEEE80211_MODE_11NG < g_ic.ic_profile.phyMode) ||
		 (g_ic.ic_profile.phyMode == IEEE80211_MODE_AUTO)) {
		g_ic.ic_profile.phyMode = IEEE80211_MODE_11NG;
	}
	if (g_ic.ic_profile.minimum_rssi_in_fast_scan == -1) {
		g_ic.ic_profile.minimum_rssi_in_fast_scan = -0x7f;
	}
	if (g_ic.ic_profile.minimum_auth_mode_in_fast_scan == 0xff) {
		g_ic.ic_profile.minimum_auth_mode_in_fast_scan = '\0';
	}
	if (bVar1) {
		system_param_save_with_protect((uint16)system_param_sector_start,&g_ic.ic_profile,0x4a4);
	}
}

void
spi_flash_clk_sel(int speed)
{
	bool swapped = (DPORT->PERI_IO_SWAP & 2) != 0;
	uint32_t clkcnt;

	/* FRE(SCLK) = clk_equ_sysclk ? 80MHz : APB_CLK(80MHz) / clkdiv_pre / clkcnt */

	if (speed < 2) {
		/* SPI_80MHz_DIV = 1 */
		SPI0->CTRL |= 0x1000;
		if (!swapped) {
			/* SPI not swapped */
			IOMUX->CONF |= 0x100; /* set SPI0_CLK_EQU_SYS_CLK */
		}
		else {
			/* SPI is swapped */
			IOMUX->CONF |= 0x200; /* set SPI1_CLK_EQU_SYS_CLK */
		}
		clkcnt = 0x1000;
	}
	else {
		int iVar3 = speed + 1;
		if (-1 < speed) {
			iVar3 = speed;
		}
		clkcnt = ((speed - 1) * 0x101 + ((iVar3 >> 1) + -1) * 0x10);
		SPI0->CTRL &= ~0x1000;
		if (!swapped) {
			/* SPI not swapped */
			IOMUX->CONF &= ~0x100; /* clear SPI0_CLK_EQU_SYS_CLK */
		}
		else {
			/* SPI is swapped */
			IOMUX->CONF &= ~0x200; /* clear SPI1_CLK_EQU_SYS_CLK */
		}
	}
	SPI0->CTRL = (SPI0->CTRL & ~0xfff) | clkcnt;
}

bool __attribute__((weak))
system_correct_flash_map(void)
{
	return false;
}

/*
         U _bss_end
         U _bss_start
         U Cache_Read_Enable
         U chip_v6_set_chan_offset
         U cnx_attach
         U done_cb
         U espconn_init
         U ets_bzero
         U ets_delay_us
         U ets_install_putc1
         U ets_intr_lock
         U ets_isr_attach
         U ets_isr_unmask
         U ets_memcpy
         U ets_memset
         U ets_printf
         U ets_run
         U ets_timer_arm_new
         U ets_timer_disarm
         U ets_timer_init
         U ets_timer_setfn
         U flashchip
         U fpm_attach
         U get_data_from_rtc
         U g_ic
         U ieee80211_ifattach
         U ieee80211_phy_init
         U _irom0_text_start
         U lmacInit
         U load_non_32_wide_handler
         U lwip_init
         U memcmp
         U netif_set_default
         U os_printf_plus
         U phy_afterwake_set_rfoption
         U phy_check_data_table
         U phy_disable_agc
         U phy_enable_agc
         U phy_get_bb_evm
         U phy_rx_gain_dc_table
         U pm_attach
         U pp_attach
         U pp_soft_wdt_init
         U pvPortZalloc
         U register_chipv6_phy
         U rst_if
         U rtc_get_reset_reason
         U sleep_reset_analog_rtcreg_8266
         U SPIEraseSector
         U spi_flash_read
         U SPIRead
         U SPIWrite
         U sys_check_timeouts
         U system_get_checksum
         U system_get_time
         U system_param_save_with_protect
         U system_param_sector_start
         U system_restart_core
         U system_restart_local
         U system_rtc_mem_read
         U system_rtc_mem_write
         U system_save_sys_param
         U system_uart_de_swap
         U system_uart_swap
         U TestStaFreqCalValDev
         U TestStaFreqCalValInput
         U __udivsi3
         U user_init
         U user_pre_init
         U user_spi_flash_dio_to_qio_pre_init
         U vPortFree
         U Wait_SPI_Idle
         U wDevEnableRx
         U wDev_Initialize
         U wDev_ProcessFiq
         U wDev_Set_Beacon_Int
         U wifi_mode_set
         U wifi_param_save_protect_with_check
         U wifi_softap_cacl_mac
         U wifi_softap_set_default_ssid
         U wifi_softap_start
         U wifi_station_connect
         U wifi_station_get_auto_connect
         U wifi_station_set_default_hostname
         U wifi_station_start
         U write_data_to_rtc
         U xthal_set_intclear
         U _xtos_set_exception_handler
00000000 d cache_map_76
00000000 T esp_crc8
00000000 t mem_debug_file_152
00000000 b NmiTimerCb_80
00000000 R SDK_VERSION
00000000 W system_correct_flash_map
00000000 t userbin_check
00000000 W user_iram_memory_is_enabled
00000004 B info
00000008 T call_user_start_local
00000008 T NmiTimSetFunc
0000000c t irom_printf
0000000c t user_uart_write_char
0000000c T wdt_feed
00000010 T Cache_Read_Enable_New
00000010 t esp_init_default_data_204
00000010 T NMI_Handler
00000014 T uart_div_modify
00000014 t user_fatal_exception_handler
00000014 t user_uart_tx_one_char
0000001c t spi_flash_clk_sel
00000024 t get_bin_len
00000028 B freq_trace_enable
0000002a B lwip_timer_interval
0000002c B check_timeouts_timer
00000040 B system_phy_init_sector
00000044 B system_rf_cal_sector
00000048 B user_init_flag
00000090 t flash_str$6812_60_8
00000090 t read_macaddr_from_otp
000000c0 t flash_str$6814_60_9
000000d0 t flash_str$6820_60_14
000000d8 t flash_str$6822_60_16
000000e0 t flash_str$6823_60_17
000000e4 t flash_str$6824_60_18
000000e8 t flash_str$6893_62_4
000000f0 t flash_str$7091_92_26
00000110 t flash_str$7093_92_28
00000124 t user_start
00000130 t flash_str$7096_92_29
00000150 t flash_str$7098_92_30
00000170 t flash_str$7100_92_31
00000190 t flash_str$7102_92_32
000001b0 t flash_str$7104_92_33
000001c0 t flash_str$7106_92_34
000001e0 t flash_str$7108_92_35
000001f0 t chip_init
00000200 t flash_str$7110_92_36
00000220 t flash_str$7112_92_37
00000240 t flash_str$7114_92_38
00000260 t flash_str$7116_92_39
00000280 t flash_str$7118_92_40
000002a0 t flash_str$7120_92_41
000002c0 t flash_str$7122_92_42
000002e0 t flash_str$7124_92_43
000002e8 t user_init_default_profile
00000300 t flash_str$7126_92_44
00000320 t flash_str$7128_92_45
00000340 t flash_str$7130_92_46
00000360 t flash_str$7132_92_47
00000380 t flash_str$7134_92_48
000003a0 t flash_str$7136_92_49
000003c0 t flash_str$7138_92_50
000003d0 t flash_str$7139_92_51
00000420 t flash_str$7292_102_4
00000430 t flash_str$7294_102_5
00000444 T wdt_init
00000450 t flash_str$7295_102_6
00000458 t flash_str$7296_102_7
00000460 t flash_str$7321_102_16
00000480 t flash_str$7341_102_20
000004a0 t flash_str$7342_102_21
000004c0 t flash_str$7343_102_22
000004d0 t flash_str$7344_102_23
000004e0 t flash_str$7345_102_24
000004f0 t flash_str$7346_102_25
000004f0 T user_uart_wait_tx_fifo_empty
00000500 t flash_str$7350_102_27
00000530 t flash_str$7351_102_28
00000550 t flash_str$7697_139_2
00000580 t flash_str$7786_57_13
0000061c t ets_fatal_exception_handler
0000065a t __switchjump_table_xs_92_59
0000080c W wifi_set_backup_mac
00000814 T system_phy_freq_trace_enable
00000830 T flash_data_check
00000ae4 t user_local_init
00001108 t print_system_param_sectors
00001128 t init_bss_data
*/

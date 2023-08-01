/* Based on:
 * - https://github.com/pvvx/esp8266web/blob/master/info/libs/main/startup.c
 * - https://github.com/SmingHub/Sming/blob/develop/Sming/Arch/Esp8266/Components/esp_no_wifi/app_main.c
 * - Ghidra decompilation
 */
#include <assert.h>
#include "c_types.h"
#include "spi_flash.h"
#include "osapi.h"
#include "mem.h"

#include "relib/esp8266.h"
#include "relib/xtensa.h"
#include "relib/ets_rom.h"
#include "relib/ets_timer.h"
#include "relib/relib.h"
#include "relib/hooks.h"
#include "relib/efuse_register.h"
#include "relib/phy_ops.h"
#include "relib/s/ieee80211_conn.h"
#include "relib/s/ieee80211com.h"
#include "relib/s/wl_profile.h"
#include "relib/s/phy_init_ctrl.h"
#include "relib/s/partition_item.h"
#include "relib/s/spi_flash_header.h"
#include "relib/s/wifi_flash_header.h"
#include "relib/s/rst_info.h"
#include "relib/s/if_param.h"
#include "relib/s/opmode.h"

#define BOOT_CLK_FREQ   52000000
#define NORMAL_CLK_FREQ 80000000
#define UART_BAUDRATE   115200

#if 0
#define DPRINTF(...) do { \
	ets_printf(__VA_ARGS__); \
	while ((UART0->STATUS & 0xff0000) != 0); \
} while (0)
#else
#define DPRINTF(...)
#endif

typedef struct __attribute__((packed)) {
	union {
		uint8_t pad[0x274];
		struct {
			uint8_t pad1[0x80];
			uint8_t rx_gain_dc_table[0x100]; /* exact size not yet clear */
		};
	};
} rf_cal_st;
static_assert(sizeof(rf_cal_st) == 0x274, "rf_cal size mismatch");

typedef struct __attribute__((packed)) {
	esp_init_data_default_st esp_init;
	union {
		rf_cal_st rf_cal;
	};
} phy_init_and_rf_cal_st;
static_assert(sizeof(phy_init_and_rf_cal_st) == (0x2f4), "phy_init_and_rf_cal_st size mismatch");
static_assert(OFFSET_OF(phy_init_and_rf_cal_st, rf_cal.rx_gain_dc_table) == 0x100, "rx_gain_dc_table offset mismatch");

bool system_correct_flash_map(void); /* always returns false, weak symbol? */
bool system_rtc_mem_read(uint8_t src_addr, void *des_addr, uint16_t load_size);
bool system_rtc_mem_write(uint8_t des_addr, const void *src_addr, uint16_t save_size);
int flash_data_check(uint8_t *src);
int phy_check_data_table(void * gdctbl, int x, int flg);
int read_macaddr_from_otp(uint8_t *mac);
extern int system_param_sector_start;
int user_iram_memory_is_enabled(void);
int wifi_mode_set(int mode);
int wifi_station_start(void);
int wifi_softap_start(int);
int wifi_softap_stop(int);
bool wifi_station_connect(void);
uint8_t wifi_station_get_auto_connect(void);
uint8_t esp_crc8(uint8_t *src, int len);
void chip_init(esp_init_data_default_st *init_data, uint8_t *mac);
void chip_v6_set_chan_offset(int, int);
void espconn_init(void);
void load_non_32_wide_handler(xtos_exception_frame_t *ef, int cause);
void lwip_init(void);
void netif_set_default(struct netif *netif);
void phy_afterwake_set_rfoption(int opt);
uint32_t phy_get_bb_evm(void);
void rom_i2c_writeReg(uint8_t block, uint8_t host_id, uint8_t reg_add, uint8_t data);
void sleep_reset_analog_rtcreg_8266(void);
void spi_flash_clk_sel(int speed);
void sys_check_timeouts(void *timer_arg);
void system_save_sys_param(void);
void system_uart_de_swap(void);
void system_uart_swap(void);
void system_restart_local(void);
void uart_div_modify(uint8 uart_no, uint32 DivLatchValue);
void user_fatal_exception_handler(xtos_exception_frame_t *ef, int cause);
void user_init(void);
void user_init_default_profile(void);
void user_local_init(void);
void user_pre_init(void);
void user_spi_flash_dio_to_qio_pre_init(void);
void user_start(void);
void user_uart_wait_tx_fifo_empty(uint32_t ch, uint32_t wait_micros);
void user_uart_write_char(char c);
void wdt_init(bool hw_wdt_enable);
void wifi_set_backup_mac(const uint8_t *src);
void wifi_softap_cacl_mac(uint8_t *dst, const uint8_t *src);
void write_data_to_rtc(uint8_t*);
void get_data_from_rtc(uint8_t*);
void wifi_param_save_protect_with_check(uint16_t startsector, int sectorsize, void *pdata, uint16_t len);

typedef void (*init_done_cb_t)(void);

extern rst_info_st rst_if; /* user_interface.o */
extern if_param_st info; /* defined in app_main.c */
extern ieee80211com_st g_ic; /* defined in ieee80211.c */
extern uint16_t lwip_timer_interval; /* app_main.o */
extern uint8_t _bss_start;
extern uint8_t _bss_end;
extern uint32_t system_phy_init_sector; /* app_main.o */
extern uint32_t system_rf_cal_sector; /* app_main.o */
extern ETSTimer check_timeouts_timer; /* app_main.o */
extern uint8_t *phy_rx_gain_dc_table; /* phy_chip_v6.o */
extern uint8_t freq_trace_enable; /* app_main.o */
extern uint16_t TestStaFreqCalValInput; /* ieee80211_scan.o */
extern uint16_t TestStaFreqCalValDev; /* ieee80211_scan.o */
extern uint8_t user_init_flag; /* app_main.o */
extern init_done_cb_t done_cb; /* user_interface.o */

/* Cache_Read_Enable:
  mb_region = 0, odd_even = 0 -> map first 8Mbit of flash
  mb_region = 0, odd_even = 1 -> map second 8Mbit of flash
  mb_region = 1, odd_even = 0 -> map third 8Mbit of flash
  mb_region = 1, odd_even = 1 -> map fourth 8Mbit of flash
  cache_size:
     SOC_CACHE_SIZE 0 // 16KB
     SOC_CACHE_SIZE 1 // 32KB
 */
void Cache_Read_Enable(uint8_t odd_even, uint8_t mb_region, uint8_t cache_size);
void Cache_Read_Disable(void);

static bool cache_map;

static void
relib_reset_uart_baud(int clk)
{
	/* Wait for UART tx fifo to drain */
	while ((UART0->STATUS & 0xff0000) != 0);
#if 0
	/* Upclock PLL to 80MHz AHB freq */
	rom_i2c_writeReg(103, 4, 1, 0x88);
	rom_i2c_writeReg(103, 4, 2, 0x91);
	/* uart_div_modify also resets the fifos, but we already flushed it */
	UART0->CLKDIV = NORMAL_CLK_FREQ / UART_BAUDRATE;
	/* Wait a bit for clock to stabilize */
	ets_delay_us(1000);
#else
	UART0->CLKDIV = clk / UART_BAUDRATE;
#endif
}

extern int chip_version;

int ICACHE_FLASH_ATTR
get_chip_version(void)
{
	if (chip_version == -1) {
		uint32_t x;
		if (EFUSE->DATA[2] & 0x8000) {
			x = EFUSE->DATA[1];
		} else {
			x = EFUSE->DATA[3];
		}
		chip_version = (x >> 24) & 0xf;
	}

	return chip_version;
}

void ICACHE_FLASH_ATTR
sleep_opt_8266(uint32_t r1, uint32_t sw_reset, uint32_t reset_ctl)
{
	RTC->R40 = 0;
	RTC->R44 = r1;
	RTC->SW_RESET = sw_reset;
	RTC->RESET_CTL = reset_ctl;
}

void ICACHE_FLASH_ATTR
sleep_opt_bb_on_8266(void)
{
	if (get_chip_version() == 2) {
		sleep_opt_8266(0, 0, 0x00800000);
	} else {
		sleep_opt_8266(0, 0, 0x00800050);
	}
}

void ICACHE_FLASH_ATTR
pm_wakeup_opt(uint32_t reg18, uint32_t rega8)
{
	RTC->WAKEUP_HW_CAUSE_REG = (RTC->WAKEUP_HW_CAUSE_REG & ~0x3f) | reg18;
	RTC->RA8 = (RTC->RA8 & ~1) | rega8;
}

void ICACHE_FLASH_ATTR
pm_set_pll_xtal_wait_time(void)
{
	if (get_chip_version() < 2) {
		RTC->XTAL_WAIT_TIME = 0xc8064;
	} else {
		RTC->XTAL_WAIT_TIME = 0x3203c;
	}
}

extern uint8_t periodic_cal_sat;

void ICACHE_FLASH_ATTR
pm_set_sleep_cycles(int rtc_cycles)
{
	/* Set the target value of RTC_COUNTER for wakeup from light-sleep/deep-sleep */
	RTC->SLP_VAL /* target */ = rtc_cycles + RTC->SLP_CNT_VAL /* current count */;
	periodic_cal_sat = 5000 < rtc_cycles;
}

extern uint8_t software_slp_reject;;
extern uint8_t hardware_reject;

void ICACHE_FLASH_ATTR
pm_wait4wakeup(int param)
{
	if (!(((param == 1) || (param == 2)) && (software_slp_reject == 0))) {
		return;
	}
	while ((RTC->SLP_STATE & 3) == 0);
	hardware_reject = RTC->SLP_STATE & 2;
}

void ICACHE_FLASH_ATTR
sleep_reset_analog_rtcreg_8266(void)
{
	RTC->INT_CLR = 0xffffffff;
	sleep_opt_bb_on_8266();
	pm_wakeup_opt(8,0);
	pm_set_pll_xtal_wait_time();
	pm_set_sleep_cycles(1000);

	RTC->SLP_CTL |= 0x100000; /* trigger wakeup? */
	pm_wait4wakeup(2);

	RTC->SW_RESET = 0x0019c06a;

	/* Note: This overrides the value set by pm_set_sleep_cycles() above */
	RTC->SLP_VAL = 0xfff;
	RTC->SLP_CTL = 0;

	/* Note: This overrides the value set by pm_set_pll_xtal_wait_time() above */
	RTC->XTAL_WAIT_TIME = 0x640c8;

	RTC->RESET_CTL = 0xf0000000;
	RTC->WAKEUP_HW_CAUSE_REG = 4;
	RTC->INT_ENA = 0;
	RTC->R40 = 0;
	RTC->R44 = 0;
	RTC->R48 = 0x20302020;
	RTC->R4C = 0x20500000;
	RTC->R58 = 0;
	RTC->R5C = 7;
	RTC->R60 = 7;
	RTC->R64 = 0;
	RTC->GPIO_OUT = 0;
	RTC->GPIO_ENABLE = 0;
	RTC->R80 = 0;
	RTC->GPIO_CONF = 0;
	RTC->R94 = 0;
	RTC->R98 = 0;
	RTC->R9C = 0;
	RTC->XPD_DCDC_CONF = 0;
	RTC->RA8 = 0;
	RTC->RAC = 0;
	RTC->RB0 = 0;
	RTC->RB4 = 0;
}

static bool
userbin_check(struct boot_hdr *bhdr)
{
	if (bhdr->use_bin == 1) {
		if (bhdr->boot_status == 0) {
			return 0;
		}
	} else if (bhdr->boot_status != 0) {
		return 0;
	}
	return 1;
}

void
Cache_Read_Enable_New(void)
{
	int user_iram = user_iram_memory_is_enabled(); /* returns 0, weak symbol? */
	Cache_Read_Enable(cache_map, 0, !user_iram);
}

static const char *exceptions[30] = {
	"IllegalInstruction",
	"Syscall",
	"InstructionFetchError",
	"LoadStoreError",
	"Level1Interrupt",
	"Alloca",
	"IntegerDivideByZero",
	"Reserved",
	"Privileged",
	"LoadStoreAlignment",
	"Reserved10",
	"Reserved11",
	"InstrPIFDateError",
	"LoadStorePIFDataError",
	"InstrPIFAddrError",
	"LoadStorePIFAddrError",
	"InstTLBMiss",
	"InstTLBMultiHit",
	"InstFetchPrivilege",
	"Reserved19",
	"InstFetchProhibited",
	"Reserved21",
	"Reserved22",
	"Reserved23",
	"LoadStoreTLBMiss",
	"LoadStoreTLBMultiHit",
	"LoadStorePrivilege",
	"Reserved27",
	"LoadProhibited",
	"StoreProhibited",
};

void ICACHE_FLASH_ATTR
ets_fatal_exception_handler(xtos_exception_frame_t *ef, int cause)
{
	uint32_t excvaddr = RSR(EXCVADDR);
	/* UserExceptionVector reserves 0x100 bytes of stack, and
	 * stores the exception frame at the start of it. */
	uint32_t sp = 0x100 + (uint32_t)ef;

	os_printf_plus("\nFatal exception %d (%s)\n", cause, exceptions[cause]);

	/* Dump registers */
	os_printf_plus("epc=%08x excvaddr=%08x ps=%08x\n",
		ef->epc, excvaddr, ef->ps);
	os_printf_plus("a0=%08x esp=%08x a2=%08x  a3=%08x  a4=%08x  a5=%08x  a6=%08x  a7=%08x\n",
		ef->a0, sp, ef->a2, ef->a3, ef->a4, ef->a5, ef->a6, ef->a7);
	os_printf_plus("a8=%08x a9=%08x a10=%08x a11=%08x a12=%08x a13=%08x a14=%08x a15=%08x\n",
		ef->a8, ef->a9, ef->a10, ef->a11, ef->a12, ef->a13, ef->a14, ef->a15);

	/* Dump stack */
	sp &= ~31;
	while (sp > 0x3ffc0000 && sp < 0x40000000) {
		uint32_t *p = (void*)sp;
		os_printf_plus("%08x: %08x %08x %08x %08x %08x %08x %08x %08x\n",
			p, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
		sp += 32;
	}

	RTC->STORE[0] = 2;
#if 1
	while (1);
#else
	system_rtc_mem_write(false, &exc_regs, 0x1c);
	system_restart_local();
#endif
}

void
user_fatal_exception_handler(xtos_exception_frame_t *ef, int cause)
{
	LOCK_IRQ_SAVE();
	Wait_SPI_Idle(flashchip);
	Cache_Read_Enable_New();
	ets_fatal_exception_handler(ef, cause);
}

static uint8_t
system_get_checksum(void *src, int len)
{
	uint8_t csum = 0xef;
	uint8_t *p = src;

	while (len--) {
		csum ^= *p++;
	}
	return csum;
}

static void
print_system_param_sectors(int start_sector)
{
	ets_printf("system param error, use last saved param!\n");
}

static void
init_bss_data(void)
{
	/* Zero BSS section */
	ets_bzero(&_bss_start, &_bss_end - &_bss_start);
}

int register_chipv6_phy(esp_init_data_default_st *init_data);
void phy_disable_agc(void);
void ieee80211_phy_init(ieee80211_phymode_t phyMode);
void lmacInit(void);
void wDev_Initialize(void);
void pp_attach(void);
void pp_soft_wdt_init(void);
void ieee80211_ifattach(ieee80211com_st *ic, uint8_t *macaddr);
void wDev_ProcessFiq(void *unused_arg);
void relib_wDev_ProcessFiq(void *unused_arg);
void pm_attach(void);
void fpm_attach(void);
void phy_enable_agc(void);
void cnx_attach(ieee80211com_st *ic);
void wDevEnableRx(void);

void relib_pp_attach(void);

void ICACHE_FLASH_ATTR
chip_init(esp_init_data_default_st *init_data, uint8_t *macaddr)
{
	DPRINTF("chip_init\n");
	if (register_chipv6_phy(init_data) != 0) {
		os_printf_plus("%s %u\n", "mai", 0x130);
		while (1);
	}
#if 0
	uart_div_modify(0, NORMAL_CLK_FREQ / UART_BAUDRATE);
	uart_div_modify(1, NORMAL_CLK_FREQ / UART_BAUDRATE);
#else
	relib_reset_uart_baud(NORMAL_CLK_FREQ);
#endif
	phy_disable_agc();
	ieee80211_phy_init(g_ic.ic_profile.phyMode);
	lmacInit();
	debug_trace((uint32_t)wDev_Initialize);
	wDev_Initialize();
	relib_pp_attach();
	ieee80211_ifattach(&g_ic, macaddr);
	_xtos_set_interrupt_handler_arg(0, relib_wDev_ProcessFiq, NULL);
	_xtos_ints_on(1);
	pm_attach();
	fpm_attach();
	phy_enable_agc();
	cnx_attach(&g_ic);
	wDevEnableRx();
}

void wifi_softap_set_default_ssid(void);
void wifi_station_set_default_hostname(uint8_t *mac);
void wDev_Set_Beacon_Int(uint32_t beacon_int);
bool system_param_save_with_protect(uint16_t start_sec, void *param, uint16_t len);

#if 1
void ICACHE_FLASH_ATTR
relib_user_local_init(void)
{
	uint8_t bVar1;
	char cVar2;
	bool bVar3;
	int iVar5;
	uint32_t uVar6;
	phy_init_and_rf_cal_st *phy_rf_data;
	char *rf_cal_data;
	char cVar8;
	char cVar9;

	_xtos_set_exception_handler(9,user_fatal_exception_handler);
	_xtos_set_exception_handler(0,user_fatal_exception_handler);
	_xtos_set_exception_handler(2,user_fatal_exception_handler);
	_xtos_set_exception_handler(3,load_non_32_wide_handler);
	_xtos_set_exception_handler(0x1c,user_fatal_exception_handler);
	_xtos_set_exception_handler(0x1d,user_fatal_exception_handler);
	_xtos_set_exception_handler(8,user_fatal_exception_handler);
	DPRINTF("sleep_reset_analog_rtcreg_8266\n");
	sleep_reset_analog_rtcreg_8266();  /* resets clocks, so re-init uart after */
	relib_reset_uart_baud(BOOT_CLK_FREQ);
	user_pre_init();
	iVar5 = read_macaddr_from_otp(info.sta_mac);
	if (iVar5 == 0) {
		iVar5 = memcmp(info.sta_mac,g_ic.ic_profile.mac_bakup,6);
		if (iVar5 != 0) {
			memcpy(g_ic.ic_profile.mac_bakup,info.sta_mac,6);
			os_printf_plus("Backup\n");
			system_save_sys_param();
		}
	}
	else {
		bVar1 = g_ic.ic_profile.mac_bakup[0] & 1;
		if ((g_ic.ic_profile.mac_bakup[0] & 1) == 0) {
			iVar5 = memcmp(info.sta_mac,g_ic.ic_profile.mac_bakup,6);
			if (iVar5 == 0) {
				os_printf_plus("Ce\n");
			}
			else if (bVar1 == 0) {
				memcpy(info.sta_mac,g_ic.ic_profile.mac_bakup,6);
				os_printf_plus("Load\n");
			}
		}
		else {
			if (iVar5 == -1) {
				info.sta_mac[0] = '\\';
				info.sta_mac[1] = 0xcf;
				info.sta_mac[2] = '\x7f';
			}
			memcpy(g_ic.ic_profile.mac_bakup,info.sta_mac,6);
			os_printf_plus("Backup default %d\n",iVar5);
			system_save_sys_param();
		}
	}
	wifi_set_backup_mac(info.sta_mac);
	wifi_softap_cacl_mac(info.softap_mac, info.sta_mac);
	info.softap_info.ip.addr = 0x104a8c0; /* 192.168.4.1 */
	info.softap_info.gw.addr = 0x104a8c0;
	info.softap_info.netmask.addr = 0xffffff;
	DPRINTF("ets_timer_init\n");
	ets_timer_init();
	DPRINTF("lwip_init\n");
	lwip_init();
	espconn_init();
	lwip_timer_interval = 25; /* 25ms */
	ets_timer_setfn(&check_timeouts_timer,sys_check_timeouts,0);
	DPRINTF("user_init_default_profile\n");
	user_init_default_profile();
	if ((DPORT->PERI_IO_SWAP & 1) == 0) {
		system_uart_de_swap();
	} else {
		system_uart_swap();
	}
	uVar6 = RTC->STORE[0];
	while ((UART0->STATUS & 0xff0000) != 0);
	while ((UART1->STATUS & 0xff0000) != 0);
	IOMUX->GPIO0 &= 0xfffffeff;  /* Switch from CLK_OUT to GPIO */
	IOMUX->GPIO2 &= 0xfffffeff;  /* Switch from U0TXD_BK to GPIO */
	system_rtc_mem_read(0,&rst_if,0x1c);
	if (uVar6 == 0) {
		uVar6 = rtc_get_reset_reason();
		if (uVar6 == 1) {
			memset(&rst_if,0,0x1c);
		}
		else {
			uVar6 = rtc_get_reset_reason();
			if (uVar6 == 2) {
				if (((rst_if.reason != REASON_DEEP_SLEEP_AWAKE) || (rst_if.epc1 != 0)) || (rst_if.excvaddr != 0)) {
					memset(&rst_if,0,0x1c);
					rst_if.reason = REASON_EXT_SYS_RST;
				}
			}
			else {
				rtc_get_reset_reason();
			}
		}
	}
	else if (6 < uVar6) {
		memset(&rst_if,0,0x1c);
	}
	system_rtc_mem_write(0,&rst_if,0x1c);
	DPRINTF("read phy and rf cal data\n");
	phy_rf_data = os_zalloc_dram(sizeof(*phy_rf_data));
	spi_flash_read(flashchip->sector_size * system_phy_init_sector,(void*)phy_rf_data,0x80);
	rf_cal_data = (void*)&phy_rf_data->rf_cal;
	spi_flash_read(flashchip->sector_size * system_rf_cal_sector,(void*)rf_cal_data,0x274);
	phy_rx_gain_dc_table = phy_rf_data->rf_cal.rx_gain_dc_table;
	iVar5 = flash_data_check(rf_cal_data);
	if ((iVar5 == 0) && (iVar5 = phy_check_data_table(phy_rx_gain_dc_table,0x7d,1), iVar5 == 0)) {
		bVar3 = false;
	}
	else {
		bVar3 = true;
	}
	if (rst_if.reason != REASON_DEEP_SLEEP_AWAKE) {
		phy_afterwake_set_rfoption(1);
	}
	if ((rst_if.reason != REASON_DEEP_SLEEP_AWAKE) && (!bVar3)) {
		write_data_to_rtc(rf_cal_data);
	}
	if (phy_rf_data->esp_init.param_ver_id == '\x05') {
		cVar9 = '\v';
		cVar2 = phy_rf_data->esp_init.freq_offset;
		if (freq_trace_enable == false) {
			uVar6 = phy_rf_data->esp_init._112;
			if (((uVar6 < 2) && (-1 < (int)uVar6)) || (uVar6 == 3)) {
LAB_4022f98c:
				phy_rf_data->esp_init._112 = '\0';
			}
			else {
				cVar9 = '\a';
				if (uVar6 == 5) {
					if (-1 < cVar2) {
						if (cVar2 < '\a') {
							cVar9 = '\0';
							phy_rf_data->esp_init.freq_offset = '\0';
						}
						else {
							cVar9 = '\x05';
						}
					}
				}
				else if (uVar6 == 7) {
					if (-1 < cVar2) {
						cVar9 = '\0';
						phy_rf_data->esp_init.freq_offset = '\0';
					}
				}
				else if (uVar6 == 9) {
					if (-1 < cVar2) {
						if (cVar2 < '\a') {
							cVar9 = '\0';
							phy_rf_data->esp_init.freq_offset = '\0';
						}
						else {
							cVar9 = '\x05';
						}
					}
				}
				else {
					if (uVar6 != 0xb) goto LAB_4022f98c;
					if (-1 < cVar2) {
						cVar9 = '\0';
						phy_rf_data->esp_init.freq_offset = '\0';
					}
				}
				phy_rf_data->esp_init._112 = cVar9;
			}
		}
		else if (freq_trace_enable == true) {
			bVar1 = phy_rf_data->esp_init._112;
			cVar8 = '\x03';
			if ((1 < bVar1) && (bVar1 != 3)) {
				if (bVar1 == 5) {
					cVar8 = cVar9;
					if ((-1 < cVar2) && (cVar8 = '\t', cVar2 < '\a')) {
						phy_rf_data->esp_init.freq_offset = '\0';
						cVar8 = '\x03';
					}
				}
				else if (bVar1 == 7) {
					cVar8 = cVar9;
					if (-1 < cVar2) {
						phy_rf_data->esp_init.freq_offset = '\0';
						cVar8 = '\x03';
					}
				}
				else if (bVar1 == 9) {
					cVar8 = cVar9;
					if (-1 < cVar2) {
						if (cVar2 < '\a') {
							phy_rf_data->esp_init.freq_offset = '\0';
							cVar8 = '\x03';
						}
						else {
							cVar8 = '\t';
						}
					}
				}
				else if ((bVar1 == 0xb) && (cVar8 = cVar9, -1 < cVar2)) {
					phy_rf_data->esp_init.freq_offset = '\0';
					cVar8 = '\x03';
				}
			}
			phy_rf_data->esp_init._112 = cVar8;
		}
		chip_init(&phy_rf_data->esp_init, info.sta_mac);  /* resets clocks, so re-init uart */
	}
	else {
		os_printf_plus("rf_cal[0] !=0x05,is 0x%02X\n");
		ets_delay_us(10000);
		system_restart_local();
	}
	g_ic.phy_function = (phy_rf_data->esp_init._112 & 5U) == 1;
	os_printf_plus("rf cal sector: %d\n",system_rf_cal_sector);
	os_printf_plus("freq trace enable %d\n",freq_trace_enable);
	os_printf_plus("rf[112] : %02x\n", phy_rf_data->esp_init._112);
	os_printf_plus("rf[113] : %02x\n", phy_rf_data->esp_init.freq_offset);
	os_printf_plus("rf[114] : %02x\n", phy_rf_data->esp_init._114);
	if ((bVar3) &&
		 (((rst_if.reason != REASON_DEEP_SLEEP_AWAKE && (uVar6 = rtc_get_reset_reason(), uVar6 == 2)) ||
			(uVar6 = rtc_get_reset_reason(), uVar6 == 1)))) {
		os_printf_plus("w_flash\n");
		get_data_from_rtc(rf_cal_data);
		wifi_param_save_protect_with_check(
			system_rf_cal_sector,
			flashchip->sector_size,
			rf_cal_data,
			0x274);
	}
	vPortFree(phy_rf_data,"app_main.c",0x53b);
	os_printf_plus("\nSDK ver: %s compiled @ Oct	9 2021 09:52:05\n",
		"3.0.5(b29dcd3)");
	uint32_t phy_ver = RTCMEM->BACKUP[0x7c/4];
	uint32_t pp_ver = RTCMEM->SYSTEM[0xf8/4];
	os_printf_plus("phy ver: %d_%d, pp ver: %d.%d\n\n",
		(phy_ver >> 0x10) & 0xfff, phy_ver >> 0x1c,
		(pp_ver & 0xff00) >> 8, pp_ver & 0xff);
	if ((g_ic.phy_function & 1) != 0) {
		uint32_t freq_cal = RTCMEM->BACKUP[0x78/4];
		if (rst_if.reason == REASON_DEEP_SLEEP_AWAKE) {
			TestStaFreqCalValInput = freq_cal >> 0x10;
			chip_v6_set_chan_offset(1, TestStaFreqCalValInput);
			TestStaFreqCalValDev = TestStaFreqCalValInput;
		}
		else {
			TestStaFreqCalValInput = 0;
			RTCMEM->BACKUP[0x78/4] = freq_cal & 0xffff;
		}
	}
	memset(&rst_if,0,0x1c);
	system_rtc_mem_write(0,&rst_if,0x1c);
	wdt_init(true);
	user_init();
	ets_timer_disarm(&check_timeouts_timer);
	ets_timer_arm_new(&check_timeouts_timer,lwip_timer_interval,1,1);
	WDT->RST = 0x73;  /* WDT_FEED() */
	user_init_flag = 1;

	install_task_hooks();

	opmode_t opmode = g_ic.ic_profile.opmode;
	wifi_mode_set(g_ic.ic_profile.opmode);
	if ((opmode == STATION_MODE) || (opmode == STATIONAP_MODE)) {
		os_printf_plus("wifi_station_start: if0=%p\n", g_ic.ic_if0_conn);
		wifi_station_start();
	}
	if (opmode == SOFTAP_MODE) {
		if (g_ic.ic_mode == 2) {
			wifi_softap_start(1);
			goto LAB_4022f927;
		}
	}
	else if (opmode != STATIONAP_MODE) goto LAB_4022f927;
	wifi_softap_start(0);
LAB_4022f927:
	if (opmode == STATION_MODE) {
		netif_set_default(g_ic.ic_if0_conn->ni_ifp);
	}
	if (wifi_station_get_auto_connect()) {
		wifi_station_connect();
	}
	if (done_cb != NULL) {
		done_cb();
	}
}
#endif

uint32_t
phy_get_bb_evm(void)
{
	I2C_SAR->reg048 &= 0xfffffffe;
	if (((EFUSE->DATA[2] >> 0xc) & 10) == 10) {
		BB->reg574 = 0xe690a568;
	}
	else {
		BB->reg574 = 0xeab4d027;
	}
	return BB->EVM >> 0x10 & 0x1fff;
}

void
fix_cache_bug(void)
{
	Cache_Read_Disable();
	phy_get_bb_evm();
	Cache_Read_Enable_New();
}

void phy_get_romfunc_addr(void);
void set_crystal_uart(void);
void ant_switch_init(void);
void phy_gpio_cfg(void);
void tx_cont_dis(void);
int register_chipv6_phy_init_param(esp_init_data_default_st *init_data);
int rtc_mem_check(int);
int rtc_mem_backup(uint8_t *mem_start, uint8_t *mem_end, uint32_t rtc_ram_off);
int rtc_mem_recovery(uint8_t *mem_start, uint8_t *mem_end, uint32_t rtc_ram_off);
void reduce_current_init(void);
void register_phy_ops(const phy_ops_st *phy_ops);
void chip_v6_set_chan(int chan);
void sleep_set_rxpbus(int arg);
void phy_version_print(void);

extern chip6_phy_init_ctrl_st chip6_phy_init_ctrl;
extern chip6_sleep_params_st chip6_sleep_params;
extern uint8_t phy_in_most_power;
extern uint16_t phy_freq_offset;
extern uint8_t init_rf_no_cal;
extern uint8_t rx_table_renew_en;
extern uint32_t test_print_time;

int chip_v6_rf_init(int ch, int n);
int chip_v6_set_chanfreq(int chfr);
void chip_v6_set_chan(int ch);
int chip_v6_unset_chanfreq(void);
void rom_chip_v5_disable_cca(void);
void rom_chip_v5_enable_cca(void);
int chip_v6_initialize_bb(void);
void chip_v6_set_sense(void);

static const phy_ops_st phy_ops = {
	chip_v6_rf_init,
	chip_v6_set_chanfreq,
	chip_v6_set_chan,
	chip_v6_unset_chanfreq,
	rom_chip_v5_enable_cca,
	rom_chip_v5_disable_cca,
	chip_v6_initialize_bb,
	chip_v6_set_sense,
};

int ICACHE_FLASH_ATTR
register_chipv6_phy(esp_init_data_default_st *init_data)
{
	bool bVar1;
	bool bVar2;
	bool bVar3;
	bool bVar4;
	bool bVar5;
	bool bVar6;
	bool bVar7;
	int iVar8;
	int iVar9;
	bool bVar10;
	uint32_t uVar11;
	uint32_t uVar12;
	char local_40;
	uint32_t uStack_3c;
	uint32_t uStack_38;
	int iStack_30;
	static bool initdone = 0;

	fix_cache_bug();
	phy_get_romfunc_addr();
	if (!initdone) {
		iStack_30 = register_chipv6_phy_init_param(init_data);
		phy_in_most_power = chip6_phy_init_ctrl.target_power_init[0];
	}
	else {
		iStack_30 = 0;
		local_40 = chip6_sleep_params._48;
	}
	set_crystal_uart();
	relib_reset_uart_baud(NORMAL_CLK_FREQ);
	ant_switch_init();
	if (!initdone) {
		phy_gpio_cfg();
	}
	tx_cont_dis();
	if ((!initdone) && ((chip6_phy_init_ctrl._76 & 0xc) != 0)) {
		phy_freq_offset = (int)chip6_phy_init_ctrl.freq_offset << 3;
	}
	uVar11 = (uint32_t)chip6_phy_init_ctrl._78;
	if (uVar11 == 2) {
		uVar11 = 1;
		chip6_phy_init_ctrl._78 = '\x01';
	}
	uVar12 = RTC->STORE[3] & 0xff;
	if (uVar12 == 0) {
		uVar12 = uVar11;
	}
	bVar4 = true;
	if ((uVar12 != 0) && (uVar12 != 1)) {
		bVar4 = false;
	}
	bVar2 = false;
	bVar1 = uVar12 == 2;
	if ((((RTC->RESET_HW_CAUSE_REG & 7) == 2) && (!initdone)) &&
		 (RTCMEM->SYSTEM[0] == 5)) {
		bVar2 = true;
	}
	bVar5 = !initdone && !bVar2;
	bVar3 = false;
	if (((bVar4) || (bVar1)) && (bVar5)) {
		bVar3 = true;
	}
	if (bVar3) {
		/* Copy 128 bytes of rfcal data to RTCMEM->BACKUP[0x00 .. 0x1f] */
		write_data_to_rtc((uint8_t *)(init_data + 0x80));
	}
	if (!initdone) {
		/* Validate the ID in the rfcal data matches EFUSE ID */
		uStack_3c = (EFUSE->DATA[2] & 0xf000) << 0x10 | EFUSE->DATA[1] & 0xfffffff;
		uStack_38 = EFUSE->DATA[0] & 0xff000000 | EFUSE->DATA[3] & 0xffffff;
		bVar3 = true;
		if ((uStack_3c == RTCMEM->BACKUP[0x60/4]) && (uStack_38 == RTCMEM->BACKUP[0x64/4])) {
			bVar3 = false;
		}
	}
	else {
		bVar3 = false;
	}
	uVar11 = RTCMEM->BACKUP[0x6c/4] >> 0x10 & 0xff;
	if (bVar2) {
		bVar1 = uVar11 == 2 || bVar1;
	}
	bVar6 = uVar11 == 4;
	if (!initdone) {
		if (bVar2) {
			uVar12 = RTCMEM->BACKUP[0x6c/4] & 0xffff;
			if ((chip6_phy_init_ctrl._72 == 0) || (uVar11 != 0)) {
				uVar11 = uVar12 + 1 & 0xffff;
			}
			else if (uVar12 < chip6_phy_init_ctrl._72) {
				bVar1 = true;
				uVar11 = uVar12 + 1 & 0xffff;
			}
			else {
				uVar11 = 0;
			}
		}
		else {
			uVar11 = 0;
		}
		RTCMEM->BACKUP[0x6c/4] = RTCMEM->BACKUP[0x6c/4] & 0xffff0000 | uVar11;
	}
	bVar10 = true;
	if (((((!bVar4) && (!bVar1)) || (initdone != '\0')) || (bVar2 && bVar6)
			) && ((initdone != '\x01' || ((~chip6_sleep_params._0_4_ & 0xff0000) == 0)))) {
		bVar10 = false;
	}
	if (bVar10) {
		iVar8 = rtc_mem_check(1);
		bVar7 = true;
		if ((iVar8 == 0) && (!bVar3)) {
			bVar7 = false;
		}
		if (bVar7) {
			bVar10 = false;
		}
	}
	if (bVar10) {
		chip6_sleep_params._96_4_ = rtc_mem_recovery((uint8_t *)&chip6_sleep_params,&chip6_sleep_params._83,0);
		rtc_mem_recovery(&chip6_sleep_params._84,&chip6_sleep_params._93,chip6_sleep_params._96_4_);
		if (!initdone) {
			if (bVar1) {
				if (bVar5) {
					chip6_sleep_params._0_4_ = chip6_sleep_params._0_4_ & 0xffbfffff;
				}
			}
			else if (bVar4) {
				chip6_sleep_params._0_4_ = 0x200000;
				chip6_sleep_params._46_2_ = 0xfe80;
			}
		}
	}
	init_rf_no_cal = 0;
	if ((bVar10) && (bVar1)) {
		init_rf_no_cal = 1;
	}
	iVar9 = phy_check_data_table(phy_rx_gain_dc_table,0x7d,1);
	iVar8 = WDEV->MICROS;
	rx_table_renew_en = '\x01';
	if ((iVar9 == 0) && ((chip6_sleep_params._0_4_ >> 0x10 & 1) != 0)) {
		rx_table_renew_en = '\0';
	}
	if (bVar2 && bVar6) {
		reduce_current_init();
	}
	else {
		register_phy_ops(&phy_ops);
	}
	test_print_time = WDEV->MICROS - iVar8;
	if (rx_table_renew_en != '\0') {
		phy_check_data_table(phy_rx_gain_dc_table,0x7d,0);
	}
	if (initdone == '\x01') {
		if ((chip6_sleep_params._0_4_ >> 0x1b & 1) == 0) {
			chip_v6_set_chan((int)local_40);
		}
	}
	else if (!bVar2 || !bVar6) {
		chip6_sleep_params._96_4_ = rtc_mem_backup((uint8_t *)&chip6_sleep_params,&chip6_sleep_params._83,0);
		rtc_mem_backup(&chip6_sleep_params._84,&chip6_sleep_params._93,chip6_sleep_params._96_4_);
		RTCMEM->BACKUP[0x60/4] = uStack_3c;
		RTCMEM->BACKUP[0x64/4] = uStack_38;
		rtc_mem_check(0);
		chip6_sleep_params._0_4_ = chip6_sleep_params._0_4_ | 0x2000000;
	}
	RTCMEM->BACKUP[0x7c/4] = RTCMEM->BACKUP[0x7c/4] & 0xffff | 0x4840000;
	if ((!bVar10) && (bVar5)) {
		get_data_from_rtc((uint8_t *)(init_data + 0x80));
	}
	/* Testmodes modifying "clear channel assesment" (CCA) regs
	if (chip6_phy_init_ctrl._59 == '\x03') {
		_DAT_60009b64 = _DAT_60009b64 & 0xfff00fff | (uint32_t)chip6_phy_init_ctrl._58 << 0xc;
	}
	if (chip6_phy_init_ctrl._59 == '\x04') {
		_DAT_60009b64 = _DAT_60009b64 & 0xfff00fff | (uint32_t)chip6_phy_init_ctrl._58 << 0xc;
		_DAT_60009d68 = _DAT_60009d68 | 0x40000;
	}
	if (chip6_phy_init_ctrl._60 == '\x02') {
		_DAT_3ff20400 = 0x55555555;
	}
	*/
	if ((chip6_sleep_params._0_4_ >> 0x1b & 1) != 0) {
		sleep_set_rxpbus(1);
	}
	if (!initdone) {
		phy_version_print();
	}
	initdone = 1;
	return iStack_30;
}

void
relib_user_start(void)
{
	spi_flash_header_st flash_hdr;
	wifi_flash_header_st hbuf;
	/* Quite a bit of stack usage here for wscfg.
	 * Fortunately we * have ~5KiB of stack at this point. */
	wl_profile_st wscfg;
	int flash_speed;
	int flash_size;
	int strap = (GPIO->IN >> 16) & 7;

	/* early uart baud init */
	relib_reset_uart_baud(BOOT_CLK_FREQ);

	/* From https://github.com/esp8266/Arduino/issues/8657 this is
	 * related to flash compatibility (`fix_cache_bug` in the NONOS SDK). */
	phy_get_bb_evm();

	ets_install_putc1(user_uart_write_char);
	DPRINTF("\nrelib_user_start:\n");

	SPI0->USER |= SPI_CK_I_EDGE; /* set rising edge (slave-mode only?) */

	DPRINTF("rom flash info:\n");
	DPRINTF("  device_id=%08x\n", flashchip->deviceId);
	DPRINTF("  chip_size=%08x\n", flashchip->chip_size);
	DPRINTF("  block_size=%08x\n", flashchip->block_size);
	DPRINTF("  sector_size=%08x\n", flashchip->sector_size);
	DPRINTF("  page_size=%08x\n", flashchip->page_size);
	DPRINTF("  status_mask=%08x\n", flashchip->status_mask);

	SPIRead(0, (void*)&flash_hdr, sizeof(flash_hdr));
	DPRINTF("flash_hdr: id=%02x segs=%d spiif=%d freq=%d map=%d\n",
		flash_hdr.id,
		flash_hdr.number_segs,
		flash_hdr.spi_interface,
		flash_hdr.spi_freq,
		flash_hdr.flash_map);

	if (strap != 5 /* not SDIO boot */) {
		if (flash_hdr.spi_freq < 3) {
			flash_speed = flash_hdr.spi_freq + 2;
		} else {
			if (flash_hdr.spi_freq == 0x0f) {
				flash_speed = 1;
			} else {
				flash_speed = 2;
			}
		}
	} else {
		flash_speed = 2;
	}

	switch(flash_hdr.flash_map) {
	case FLASH_SIZE_4M_MAP_256_256:
	default:
		/* 512KiB (256+256) */
		flash_size = 0x80000;
		break;
	case FLASH_SIZE_2M:
		/* 256KiB */
		flash_size = 0x40000;
		break;
	case FLASH_SIZE_8M_MAP_512_512:
		/* 1MiB (512+512) */
		flash_size = 0x100000;
		break;
	case FLASH_SIZE_16M_MAP_512_512:
		/* 2MiB (512+512) */
		flash_size = 0x200000;
		break;
	case FLASH_SIZE_32M_MAP_512_512:
		/* 4MiB (512+512) */
		flash_size = 0x400000;
		break;
	case FLASH_SIZE_16M_MAP_1024_1024:
		/* 2MiB (1024+1024) */
		flash_size = 0x200000;
		break;
	case FLASH_SIZE_32M_MAP_1024_1024:
		/* 4MiB (1024+1024) */
		flash_size = 0x400000;
		break;
	case FLASH_SIZE_32M_MAP_2048_2048:
		/* 4MiB (2048+2048)? */
		flash_size = 0x400000;
		break;
	case FLASH_SIZE_64M_MAP_1024_1024:
		/* 8MiB */
		flash_size = 0x800000;
		break;
	case FLASH_SIZE_128M_MAP_1024_1024:
		/* 16MiB */
		flash_size = 0x1000000;
		break;
	}
	flashchip->chip_size = flash_size;

	DPRINTF("new chip_size from hdr: %08x\n", flash_size);

	cache_map = 0;
	spi_flash_clk_sel(flash_speed);

	system_param_sector_start = (flashchip->chip_size/flashchip->sector_size) - 3;
	DPRINTF("system_param_sector_start=%08x\n", system_param_sector_start);

	uint32_t start = (system_param_sector_start + 2)*flashchip->sector_size;
	DPRINTF("SPIRead %d from @%08x\n", sizeof(hbuf), start);
	SPIRead(start, &hbuf, sizeof(hbuf));
	DPRINTF("wifi_hdr:\n");
	DPRINTF("  bank=%02x\n", hbuf.bank);
	DPRINTF("  magic1=%08x\n", hbuf.magic1);
	DPRINTF("  wr_cnt=%d\n", hbuf.wr_cnt);
	DPRINTF("  chklen0=%d\n", hbuf.chklen[0]);
	DPRINTF("  chklen1=%d\n", hbuf.chklen[1]);
	DPRINTF("  chksum0=%02x\n", hbuf.chksum[0]);
	DPRINTF("  chksum1=%02x\n", hbuf.chksum[1]);
	DPRINTF("  magic2=%08x\n", hbuf.magic2);

	start = (system_param_sector_start + (!!hbuf.bank))*flashchip->sector_size;
	DPRINTF("SPIRead %d from @%08x\n", sizeof(wscfg), start);
	SPIRead(start, &wscfg, sizeof(wscfg));
	DPRINTF("wifi_cfg:\n");
	DPRINTF("  boot.use_bin=%d\n", wscfg.boot_hdr_param.boot.use_bin);
	DPRINTF("  boot.to_qio=%d\n", wscfg.boot_hdr_param.boot.to_qio);
	DPRINTF("  boot.version=%d\n", wscfg.boot_hdr_param.boot.version);
	DPRINTF("  boot.test_bin_addr=%02x%02x%02x\n",
		wscfg.boot_hdr_param.boot.test_bin_addr[0],
		wscfg.boot_hdr_param.boot.test_bin_addr[1],
		wscfg.boot_hdr_param.boot.test_bin_addr[2]);
	DPRINTF("  boot.user_bin_addr=%02x%02x%02x\n",
		wscfg.boot_hdr_param.boot.user_bin_addr[0],
		wscfg.boot_hdr_param.boot.user_bin_addr[1],
		wscfg.boot_hdr_param.boot.user_bin_addr[2]);

	bool use_bin = userbin_check(&wscfg.boot_hdr_param.boot);
	if (flash_hdr.flash_map == FLASH_SIZE_16M_MAP_1024_1024 ||
	    flash_hdr.flash_map == FLASH_SIZE_32M_MAP_1024_1024 ||
	    flash_hdr.flash_map == FLASH_SIZE_64M_MAP_1024_1024 ||
	    flash_hdr.flash_map == FLASH_SIZE_128M_MAP_1024_1024) {
		if (wscfg.boot_hdr_param.boot_2.version < 4 ||
		    wscfg.boot_hdr_param.boot_2.version >= 32) {
			ets_printf("need boot 1.4+\n");
			while(1);
		}
		cache_map = use_bin;
	}

	if (system_correct_flash_map()) {
		ets_printf("correct flash map\r\n");
	}

	Cache_Read_Enable_New();
	init_bss_data();

	if (hbuf.magic1 == 0x55aa55aa) {
		if ((hbuf.magic2 == 0xaa55aa55) || (hbuf.magic2 == 0xffffffff)) {
			int len = hbuf.chklen[!!hbuf.bank];
			int csum = hbuf.chksum[!!hbuf.bank];
			int actual = system_get_checksum(&wscfg, len);
			if (len != 0x20 || csum != actual) {
				print_system_param_sectors(system_param_sector_start);
			} else {
				ets_printf("wscfg ok!\n");
				ets_memcpy(&g_ic.ic_profile, &wscfg, sizeof(wscfg));
			}
		} else {
			print_system_param_sectors(system_param_sector_start);
		}
	} else if (hbuf.magic1 != 0xffffffff) {
		print_system_param_sectors(system_param_sector_start);
	}
	if (wscfg.boot_hdr_param.boot.to_qio) {
		DPRINTF("user_spi_flash_dio_to_qio_pre_init\n");
		user_spi_flash_dio_to_qio_pre_init();
	}

#if 1
	install_iram_hooks();
	relib_user_local_init();
#else
	user_local_init();
#endif
}

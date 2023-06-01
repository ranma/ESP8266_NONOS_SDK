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
#include "relib/ets_rom.h"
#include "relib/relib.h"
#include "relib/efuse_register.h"
#include "relib/s/ieee80211_conn.h"

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

#define OFFSET_OF(s, f) __builtin_offsetof(s, f)
#define ARRAY_SIZE(a) (sizeof((a)) / sizeof(*(a)))

typedef struct __attribute__((packed)) {
	uint8_t id;               // = 0xE9
	uint8_t number_segs;      // Number of segments
	uint8_t spi_interface;    // SPI Flash Interface (0 = QIO, 1 = QOUT, 2 = DIO, 0x3 = DOUT)
	uint8_t spi_freq: 4;      // Low four bits: 0 = 40MHz, 1= 26MHz, 2 = 20MHz, 0x3 = 80MHz
	uint8_t flash_size: 4;    // High four bits: 0 = 512K, 1 = 256K, 2 = 1M, 3 = 2M, 4 = 4M, ...
} spi_flash_header_st;
static_assert(sizeof(spi_flash_header_st) == 0x4, "spi_flash_header_st size mismatch");

typedef struct { // Sector flash addr flashchip->chip_size-0x1000
	uint8_t bank;       // +00 = 0, 1 // WiFi config flash addr: 0 - flashchip->chip_size-0x3000 (0x7D000), 1 - flashchip->chip_size-0x2000
	uint8_t unknown[3];
	uint32_t magic1;    /* 0x55AA55AA to mark the struct as valid (0xffffffff if sector is erased) */
	uint32_t wr_cnt;    /* number of erase/write cycles so far */
	uint32_t chklen[2]; /* number of wl_profile_s bytes to checksum over */
	uint32_t chksum[2]; /* checksum of the saved wl_profile_s */
	uint32_t magic2;    /* 0xAA55AA55 to mark the struct as valid (ensure all bytes of the struct were written to flash) */
} wifi_flash_header_st;
static_assert(sizeof(wifi_flash_header_st) == 0x20, "wifi_flash_header size mismatch");

typedef struct __attribute__((packed)) {
	union {
		struct boot_hdr_2 {
			char use_bin:4;
			char flag:4;
			uint8_t version;
			uint8_t pad[6];
		} boot_2;
		struct boot_hdr {
			char use_bin:2;
			char boot_status:1;
			char to_qio:1;
			char reverse:4;

			char version:5;
			char test_pass_flag:1;
			char test_start_flag:1;
			char enhance_boot_flag:1;

			char test_bin_addr[3];
			char user_bin_addr[3];
		} boot;
	};
} boot_hdr_param_st;
static_assert(sizeof(boot_hdr_param_st) == 8, "boot_hdr_param size mismatch");

typedef struct __attribute__((packed)) {
	/* shallow struct def to avoid pulling in all the other struct members */
	union {
		uint8_t pad1[0x4a4];
		struct {
			boot_hdr_param_st boot_hdr_param;
			uint8_t opmode;
		};
		struct {
			uint8_t pad2[1170];
			uint8_t mac_bakup[6];
		};
	};
} wl_profile_st;
static_assert(sizeof(wl_profile_st) == 0x4a4, "wl_profile size mismatch");
static_assert(OFFSET_OF(wl_profile_st, opmode) == 8, "opmode offset mismatch");
static_assert(OFFSET_OF(wl_profile_st, mac_bakup) == 1170, "mac_bakup offset mismatch");

typedef struct __attribute__((packed)) {
	/* shallow struct def to avoid pulling in all the other struct members */
	union {
		uint8_t pad1[0x6b4];
		struct {
			uint8_t pad2[0x20c];
			wl_profile_st ic_profile;
		};
		struct {
			uint8_t pad3[478];
			uint8_t ic_mode;
			uint8_t phy_function;
		};
		struct {
			uint8_t pad4[16];
			ieee80211_conn_st *ic_if0_conn;
			ieee80211_conn_st *ic_if1_conn;
		};
	};
} ieee80211com_st;
static_assert(sizeof(ieee80211com_st) == 0x6b4, "ieee80211com size mismatch");
static_assert(OFFSET_OF(ieee80211com_st, ic_mode) == 478, "ic_mode offset mismatch");
static_assert(OFFSET_OF(ieee80211com_st, phy_function) == 479, "phy_function offset mismatch");
static_assert(OFFSET_OF(ieee80211com_st, ic_profile) == 0x20c, "ic_profile offset mismatch");

typedef struct __attribute__((packed)) {
	uint32_t addr;
} ip_addr_st;
static_assert(sizeof(ip_addr_st) == 4, "ip_addr size mismatch");

typedef struct __attribute__((packed)) {
	ip_addr_st ip;
	ip_addr_st netmask;
	ip_addr_st gw;
} ip_info_st;
static_assert(sizeof(ip_info_st) == 12, "ip_info size mismatch");

typedef struct __attribute__((packed)) {
	ip_info_st softap_info;
	ip_info_st sta_info;
	uint8 softap_mac[6];
	uint8 sta_mac[6];
} if_param_st;
static_assert(sizeof(if_param_st) == 0x24, "if_param size mismatch");

typedef struct {
	uint32_t flag;
	uint32_t exccause;
	uint32_t epc1;
	uint32_t epc2;
	uint32_t epc3;
	uint32_t excvaddr;
	uint32_t depc;
} rst_info_st;
static_assert(sizeof(rst_info_st) == 0x1c, "rst_info size mismatch");

typedef struct __attribute__((packed)) {
	/* shallow struct def to avoid pulling in all the other struct members */
	union {
		struct {
			uint8_t param_ver_id;
			uint8_t pad1[107];
		};
	};
} phy_init_ctrl_st;
static_assert(sizeof(phy_init_ctrl_st) == 108, "phy_init_ctrl size mismatch");

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
	union {
		phy_init_ctrl_st phy_init;
		uint8_t pad1[0x80];
	};
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
int system_param_sector_start;
int user_iram_memory_is_enabled(void);
int wifi_mode_set(int mode);
int wifi_station_start(void);
int wifi_softap_start(int);
int wifi_softap_stop(int);
bool wifi_station_connect(void);
uint8_t wifi_station_get_auto_connect(void);
uint8_t esp_crc8(uint8_t *src, int len);
void chip_init(phy_init_and_rf_cal_st *init_data, uint8_t *mac);
void chip_v6_set_chan_offset(int, int);
void espconn_init(void);
void ets_timer_init(void);
void load_non_32_wide_handler(xtos_exception_frame_t *ef, int cause);
void lwip_init(void);
void netif_set_default(struct netif *netif);
void phy_afterwake_set_rfoption(int opt);
void phy_get_bb_evm(void);
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
void wdt_init(int enable_hw_watchdog);
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

extern char jmp_hostap_deliver_data;
extern char ets_hostap_deliver_data;
extern char jmp_ieee80211_deliver_data;
extern char ets_ieee80211_deliver_data;

struct override {
	void* old_fn;
	void* new_fn;
};

static struct override overrides[] = {
	{&ets_hostap_deliver_data, &jmp_hostap_deliver_data},
	{&ets_ieee80211_deliver_data, &jmp_ieee80211_deliver_data},
};

void try_patch(uint32_t old, uint32_t new)
{
	uint32_t *data = (void*)old;
	int32_t delta = new-old;
	int32_t abs = delta ? delta : -delta;
	uint32_t op = ((delta - 4) << 6) | 0x6;
	uint32_t old_op;
	if ((old & 3) != 0) {
		ets_printf("not aligned\n");
		return;
	}
	old_op = *data;
	/* check for "addi a1, a1" */
	if ((old_op & 0xffff) != 0xc112) {
		ets_printf("unexpected function prologue %08x, skipped!\n", old_op);
		return;
	}
	if (old >= 0x40200000 || old < 0x40100000) {
		ets_printf("not in IRAM\n");
		return;
	}
	if (abs >= 131000) {
		ets_printf("%d out of jump range\n", delta);
		return;
	}
	/* It's a reasonably safe assumption that there'll be space in
	 * front for a literal, unless it is a very trivial function */
	*(data-1) = new;
	old_op = *data;
	*data = op;
	ets_printf("patched code %08x -> %08x\n", old_op, op);
}

void install_overrides(void)
{
	for (int i = 0; i < ARRAY_SIZE(overrides); i++) {
		ets_printf("Patching function %p -> %p\n",
			overrides[i].old_fn,
			overrides[i].new_fn);
		try_patch((uint32_t)overrides[i].old_fn, (uint32_t)overrides[i].new_fn);
	}
}

#if 1
/* Test: These support functions for ISSI and GD25Q32C are not small.
 * Since my flash is Winbond, it should work fine wihtout them.
 * This reduces the irom0text size from 28928 to 27696 (~1.2KiB / 4.2%).
 */
void spi_flash_issi_enable_QIO_mode(void) { }
void flash_gd25q32c_read_status(void) { }
void flash_gd25q32c_write_status(void) { }
#endif

/* Cache_Read_Enable:
  mb_region = 0, odd_even = 0 -> map first 8Mbit of flash
  mb_region = 0, odd_even = 1 -> map second 8Mbit of flash
  mb_region = 1, odd_even = 0 -> map third 8Mbit of flash
  mb_region = 1, odd_even = 1 -> map fourth 8Mbit of flash
  cache_size:
     SOC_CACHE_SIZE 0 // 16KB
     SOC_CACHE_SIZE 1 // 32KB
 */
extern void Cache_Read_Enable(uint8_t odd_even, uint8_t mb_region, uint8_t cache_size);

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

extern ETSTimer* timer_list;
extern ETSTimer* debug_timer;
static ETSEvent rtcTimerEvtQ[4];
typedef void ETSTimerFunc(void* timer_arg);
void ets_timer_intr_set(int expire);
void ets_timer_handler_isr(void);

#if 0
void ICACHE_FLASH_ATTR
ets_timer_handler_isr(void)
{
	int now;
	ETSTimer *t;
	uint32_t t_count;
	ETSTimerFunc **t_fn;
	ETSTimer **t_next;
	uint32_t *t_period;

	ets_intr_lock();
	now = WDEV_TIMER->COUNT;
	do {
		t_count = FRC2->COUNT;
		t = timer_list;
		if (timer_list == NULL) {
LAB_40230a24:
			ets_intr_unlock();
			return;
		}
		if (0 < (int)(timer_list->timer_expire - t_count)) {
			ets_timer_intr_set(timer_list->timer_expire);
			goto LAB_40230a24;
		}
		t_fn = &timer_list->timer_func;
		t_period = &timer_list->timer_period;
		debug_timer = timer_list;
		t_next = &timer_list->timer_next;
		timer_list = timer_list->timer_next;
		debug_timerfn = *t_fn;
		*t_next = (ETSTimer *)-1;
		if (*t_period != 0) {
			if ((uint)(_DAT_3ff20c00 - iVar1) < 0x3a99) {
				t_count = t->timer_expire;
			}
			t_count = *t_period + t_count;
			t->timer_expire = t_count;
			timer_insert(t_count,t);
		}
		ets_intr_unlock();
		t->timer_func(t->timer_arg);
		ets_intr_lock();
	} while( true );
}
#endif

void ICACHE_FLASH_ATTR
ets_rtc_timer_task(ETSEvent *e)
{
	if (e->sig != 0) {
		return;
	}
	ets_timer_handler_isr();
}

void ets_timer_handler_dsr(void *unused_arg)
{
	ets_post(PRIO_RTC_TIMER, 0, 0);
}

void ICACHE_FLASH_ATTR
ets_timer_init(void)
{
	timer_list = NULL;
	/* ets_isr_attach is just a no-op wrapper for * _xtos_set_interrupt_handler_arg */
	_xtos_set_interrupt_handler_arg(10, ets_timer_handler_dsr, NULL);

	DPORT->EDGE_INT_ENABLE |= 4;
	_xtos_ints_on(0x400);

	ets_task(ets_rtc_timer_task, PRIO_RTC_TIMER, rtcTimerEvtQ, ARRAY_SIZE(rtcTimerEvtQ));
	FRC2->ALARM = 0;
	FRC2->CTRL = 0x88;
	FRC2->LOAD = 0;
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

int ICACHE_FLASH_ATTR
relib_read_macaddr_from_otp(uint8_t *mac) /* impl. from RTOS SDK */
{
	uint32_t efuse[4];

	uint8_t efuse_crc = 0;
	uint8_t calc_crc = 0;
	uint8_t version;
	uint8_t use_default = 1;

	efuse[0] = EFUSE->DATA[0];
	efuse[1] = EFUSE->DATA[1];
	efuse[2] = EFUSE->DATA[2];
	efuse[3] = EFUSE->DATA[3];

	mac[3] = efuse[1] >> 8;
	mac[4] = efuse[1];
	mac[5] = efuse[0] >> 24;

	if (efuse[2] & EFUSE_IS_48BITS_MAC) {
		uint8_t tmp_mac[4];

		mac[0] = efuse[3] >> 16;
		mac[1] = efuse[3] >> 8;
		mac[2] = efuse[3];

		use_default = 0;

		tmp_mac[0] = mac[2];
		tmp_mac[1] = mac[1];
		tmp_mac[2] = mac[0];

		efuse_crc = efuse[2] >> 24;
		calc_crc = esp_crc8(tmp_mac, 3);

		if (efuse_crc != calc_crc)
			use_default = 1;

		if (!use_default) {
			version = (efuse[1] >> EFUSE_VERSION_S) & EFUSE_VERSION_V;

			if (version == EFUSE_VERSION_1 || version == EFUSE_VERSION_2) {
				tmp_mac[0] = mac[5];
				tmp_mac[1] = mac[4];
				tmp_mac[2] = mac[3];
				tmp_mac[3] = efuse[1] >> 16;

				efuse_crc = efuse[0] >> 16;
				calc_crc = esp_crc8(tmp_mac, 4);

				if (efuse_crc != calc_crc)
					use_default = 1;
			}
		}
	}

	if (use_default) {
		mac[0] = 0x18;
		mac[1] = 0xFE;
		mac[2] = 0x34;
	}

	return 0;
}

#if 1
void ICACHE_FLASH_ATTR
relib_user_local_init(void)
{
	uint8_t bVar1;
	char cVar2;
	bool bVar3;
	uint8_t uVar4;
	int iVar5;
	void *pv;
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
				if (((rst_if.flag != 5) || (rst_if.epc1 != 0)) || (rst_if.excvaddr != 0)) {
					memset(&rst_if,0,0x1c);
					rst_if.flag = 6;
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
	if (rst_if.flag != 5) {
		phy_afterwake_set_rfoption(1);
	}
	if ((rst_if.flag != 5) && (!bVar3)) {
		write_data_to_rtc(rf_cal_data);
	}
	if (phy_rf_data->phy_init.param_ver_id == '\x05') {
		cVar9 = '\v';
		cVar2 = phy_rf_data->pad1[0x71];
		if (freq_trace_enable == false) {
			uVar6 = phy_rf_data->pad1[0x70];
			if (((uVar6 < 2) && (-1 < (int)uVar6)) || (uVar6 == 3)) {
LAB_4022f98c:
				phy_rf_data->pad1[0x70] = '\0';
			}
			else {
				cVar9 = '\a';
				if (uVar6 == 5) {
					if (-1 < cVar2) {
						if (cVar2 < '\a') {
							cVar9 = '\0';
							phy_rf_data->pad1[0x71] = '\0';
						}
						else {
							cVar9 = '\x05';
						}
					}
				}
				else if (uVar6 == 7) {
					if (-1 < cVar2) {
						cVar9 = '\0';
						phy_rf_data->pad1[0x71] = '\0';
					}
				}
				else if (uVar6 == 9) {
					if (-1 < cVar2) {
						if (cVar2 < '\a') {
							cVar9 = '\0';
							phy_rf_data->pad1[0x71] = '\0';
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
						phy_rf_data->pad1[0x71] = '\0';
					}
				}
				phy_rf_data->pad1[0x70] = cVar9;
			}
		}
		else if (freq_trace_enable == true) {
			bVar1 = phy_rf_data->pad1[0x70];
			cVar8 = '\x03';
			if ((1 < bVar1) && (bVar1 != 3)) {
				if (bVar1 == 5) {
					cVar8 = cVar9;
					if ((-1 < cVar2) && (cVar8 = '\t', cVar2 < '\a')) {
						phy_rf_data->pad1[0x71] = '\0';
						cVar8 = '\x03';
					}
				}
				else if (bVar1 == 7) {
					cVar8 = cVar9;
					if (-1 < cVar2) {
						phy_rf_data->pad1[0x71] = '\0';
						cVar8 = '\x03';
					}
				}
				else if (bVar1 == 9) {
					cVar8 = cVar9;
					if (-1 < cVar2) {
						if (cVar2 < '\a') {
							phy_rf_data->pad1[0x71] = '\0';
							cVar8 = '\x03';
						}
						else {
							cVar8 = '\t';
						}
					}
				}
				else if ((bVar1 == 0xb) && (cVar8 = cVar9, -1 < cVar2)) {
					phy_rf_data->pad1[0x71] = '\0';
					cVar8 = '\x03';
				}
			}
			phy_rf_data->pad1[0x70] = cVar8;
		}
		DPRINTF("chip_init\n");
		chip_init(phy_rf_data, info.sta_mac);  /* resets clocks, so re-init uart */
		relib_reset_uart_baud(NORMAL_CLK_FREQ);
	}
	else {
		os_printf_plus("rf_cal[0] !=0x05,is 0x%02X\n");
		ets_delay_us(10000);
		system_restart_local();
	}
	g_ic.phy_function = (phy_rf_data->pad1[0x70] & 5U) == 1;
	os_printf_plus("rf cal sector: %d\n",system_rf_cal_sector);
	os_printf_plus("freq trace enable %d\n",freq_trace_enable);
	os_printf_plus("rf[112] : %02x\n", phy_rf_data->pad1[0x70]);
	os_printf_plus("rf[113] : %02x\n", phy_rf_data->pad1[0x71]);
	os_printf_plus("rf[114] : %02x\n", phy_rf_data->pad1[0x72]);
	if ((bVar3) &&
		 (((rst_if.flag != 5 && (uVar6 = rtc_get_reset_reason(), uVar6 == 2)) ||
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
		if (rst_if.flag == 5) {
			TestStaFreqCalValInput = freq_cal >> 0x10;
			chip_v6_set_chan_offset(1, TestStaFreqCalValInput);
			TestStaFreqCalValDev = TestStaFreqCalValInput;
		}
		else {
			TestStaFreqCalValInput = 0;
			RTCMEM->BACKUP[0x78/4] = freq_cal & 0xffff;
		}
	}
	system_rtc_mem_write(0,pv,0x1c);
	vPortFree(pv,"app_main.c",0x571);
	wdt_init(1);
	user_init();
	ets_timer_disarm(&check_timeouts_timer);
	ets_timer_arm_new(&check_timeouts_timer,lwip_timer_interval,1,1);
	uVar4 = g_ic.ic_profile.opmode;
	WDT->RST = 0x73;  /* WDT_FEED() */
	user_init_flag = 1;
	wifi_mode_set(g_ic.ic_profile.opmode);
	if ((uVar4 == '\x01') || (uVar4 == '\x03')) {
		wifi_station_start();
	}
	if (uVar4 == '\x02') {
		if (g_ic.ic_mode == '\x02') {
			wifi_softap_start(1);
			goto LAB_4022f927;
		}
	}
	else if (uVar4 != '\x03') goto LAB_4022f927;
	wifi_softap_start(0);
LAB_4022f927:
	if (uVar4 == '\x01') {
		netif_set_default(g_ic.ic_if0_conn->ni_ifp);
	}
	iVar5 = wifi_station_get_auto_connect();
	if (iVar5 == 1) {
		wifi_station_connect();
	}
	if (done_cb != (init_done_cb_t)0x0) {
		done_cb();
	}
	return;
}
#endif

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
	DPRINTF("flash_hdr: id=%02x segs=%d spiif=%d freq=%d size=%d\n",
		flash_hdr.id,
		flash_hdr.number_segs,
		flash_hdr.spi_interface,
		flash_hdr.spi_freq,
		flash_hdr.flash_size);

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

	switch(flash_hdr.flash_size) {
	case 0:
	default:
		/* 512KiB (256+256) */
		flash_size = 0x80000;
		break;
	case 1:
		/* 256KiB */
		flash_size = 0x40000;
		break;
	case 2:
		/* 1MiB (512+512) */
		flash_size = 0x100000;
		break;
	case 3:
		/* 2MiB (512+512) */
		flash_size = 0x200000;
		break;
	case 4:
		/* 4MiB (512+512) */
		flash_size = 0x400000;
		break;
	case 5:
		/* 2MiB (1024+1024) */
		flash_size = 0x200000;
		break;
	case 6:
		/* 4MiB (1024+1024) */
		flash_size = 0x400000;
		break;
	case 7:
		/* 4MiB (2048+2048)? */
		flash_size = 0x400000;
		break;
	case 8:
		/* 8MiB */
		flash_size = 0x800000;
		break;
	case 9:
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
	if (flash_hdr.flash_size == 5 ||
	    flash_hdr.flash_size == 6 ||
	    flash_hdr.flash_size == 8 ||
	    flash_hdr.flash_size == 9) {
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
	install_overrides();
	relib_user_local_init();
#else
	user_local_init();
#endif
}

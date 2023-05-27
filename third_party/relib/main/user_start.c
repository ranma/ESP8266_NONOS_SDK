/* Based on:
 * - https://github.com/pvvx/esp8266web/blob/master/info/libs/main/startup.c
 * - https://github.com/SmingHub/Sming/blob/develop/Sming/Arch/Esp8266/Components/esp_no_wifi/app_main.c
 * - Ghidra decompilation
 */
#include <assert.h>
#include "c_types.h"
#include "spi_flash.h"

#include "relib/esp8266.h"
#include "relib/ets_rom.h"
#include "relib/relib.h"

#if 0
#define DPRINTF(...) ets_printf(__VA_ARGS__)
#else
#define DPRINTF(...)
#endif

#define OFFSET_OF(s, f) __builtin_offsetof(s, f)

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
	union {
		boot_hdr_param_st boot_hdr_param;
		uint8_t pad1[0x4a4];
	};
} wl_profile_st;
static_assert(sizeof(wl_profile_st) == 0x4a4, "wl_profile size mismatch");

typedef struct __attribute__((packed)) {
	union {
		uint8_t pad1[0x6b4];
		struct {
			uint8_t pad2[0x20c];
			wl_profile_st ic_profile;
		};
	};
} ieee80211com_st;
static_assert(sizeof(ieee80211com_st) == 0x6b4, "ieee80211com size mismatch");

static_assert(OFFSET_OF(ieee80211com_st, ic_profile) == 0x20c, "ic_profile offset mismatch");

extern ieee80211com_st g_ic; /* defined in ieee80211.c */

extern bool system_correct_flash_map(void); /* always returns false, weak symbol? */
extern int system_param_sector_start;
extern int user_iram_memory_is_enabled(void);
extern void phy_get_bb_evm(void);
extern void rom_i2c_writeReg(uint8_t block, uint8_t host_id, uint8_t reg_add, uint8_t data);
extern void spi_flash_clk_sel(int speed);
extern void uart_div_modify(uint8 uart_no, uint32 DivLatchValue);
extern void user_local_init(void);
extern void user_spi_flash_dio_to_qio_pre_init(void);
extern void user_start(void);
extern void user_uart_wait_tx_fifo_empty(uint32_t ch, uint32_t wait_micros);
extern void user_uart_write_char(char c);
extern uint8_t _bss_start;
extern uint8_t _bss_end;

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

#if 0
	/* Upclock PLL to 80MHz AHB freq (fixes serial baud rate) */
	rom_i2c_writeReg(103, 4, 1, 0x88);
	rom_i2c_writeReg(103, 4, 2, 0x91);
	/* Wait a bit for clock to stabilize */
	ets_delay_us(1000);
#endif

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
	DPRINTF("flash_hdr: id=%02x bank=%d segs=%d spiif=%d freq=%d size=%d\n",
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

	user_local_init();
}

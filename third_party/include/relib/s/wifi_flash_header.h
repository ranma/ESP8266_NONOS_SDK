#ifndef RELIB_WIFI_FLASH_HEADER
#define RELIB_WIFI_FLASH_HEADER

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

#endif /* RELIB_WIFI_FLASH_HEADER */

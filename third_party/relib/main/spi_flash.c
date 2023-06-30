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
#include "relib/s/partition_item.h"
#include "relib/s/spi_flash_header.h"

static user_spi_flash_read flash_read;
extern bool protect_flag;

void pp_soft_wdt_feed(void);
bool spi_flash_erase_sector_check(uint16_t sec);

void
Cache_Read_Disable_2(void)
{
	DPORT->CACHE_FLASH_CTRL &= ~CACHE_READ_EN;
	while (SPI0->EXT2 != 0);
	SPI0->CTRL &= ~SPI_CTRL_ENABLE_AHB;
}

void
Cache_Read_Enable_2(void)
{
	SPI0->CTRL |= SPI_CTRL_ENABLE_AHB;
	DPORT->CACHE_FLASH_CTRL |= CACHE_READ_EN;
}

uint32_t
spi_flash_get_id(void)
{
	Cache_Read_Disable_2();
	Wait_SPI_Idle(flashchip);
	SPI0->DATA[0] = 0;
	SPI0->CMD = SPI_CMD_FLASH_RDID;
	while (SPI0->CMD != 0);
	Cache_Read_Enable_2();
	return SPI0->DATA[0] & 0xffffff;
}

SpiFlashOpResult
spi_flash_read_status(uint32 *status)
{
	SpiFlashOpResult res;
	Cache_Read_Disable_2();
	res = SPI_read_status(flashchip, status);
	Cache_Read_Enable_2();
	return res;
}

SpiFlashOpResult
spi_flash_write_status(uint32_t status_value)
{
	int iVar1;
	SpiFlashOpResult res;

	pp_soft_wdt_feed();
	Cache_Read_Disable_2();
	Wait_SPI_Idle(flashchip);

	res = SPI_write_enable(flashchip);
	if (res != SPI_FLASH_RESULT_OK) {
		goto exit_err;
	}

	res = SPI_write_status(flashchip, status_value);

exit_err:
	Wait_SPI_Idle(flashchip);
	Cache_Read_Enable_2();
	return res;
}

static void unimplemented(const char *msg)
{
	os_printf("unimplemented: %s\n", msg);
	while (1);
}

void
flash_gd25q32c_read_status(void)
{
	/* TODO: re-add */
	unimplemented("flash_gd25q32c_read_status");
}

void
flash_gd25q32c_write_status(void)
{
	/* TODO: re-add */
	unimplemented("flash_gd25q32c_write_status");
}

void
spi_flash_check_wr_protect(void)
{
	uint32_t flash_status = 0;
	uint32_t flash_id = spi_flash_get_id();
	uint32_t mfg = flash_id & 0xff;
	SpiFlashOpResult res = spi_flash_read_status(&flash_status);

	if (res != SPI_FLASH_RESULT_OK) {
		return;
	}

	if (mfg == 0x9d /* ISSI */ || mfg == 0xc2 /* MXIC */) {
		if ((flash_status & 0x3c) != 0) {
			flash_gd25q32c_write_status();
		}
		return;
	}
	else if (flash_id == 0x1640c8 || flash_id == 0x1840c8) { /* GD25Q32C / GD25Q128 */
		if ((flash_status & 0x7c) != 0) {
			flash_gd25q32c_write_status();
		}
		return;
	}

	/* default */
	flash_gd25q32c_read_status();
	if ((flash_status & 0x407c) != 0) {
		flash_status &= ~0x407c;
		spi_flash_write_status(flash_status | (1 << 8));
	}
}

SpiFlashOpResult
spi_flash_read(uint32_t src_addr, uint32_t *dst_addr, uint32_t size)
{
	if (dst_addr == NULL) {
		return SPI_FLASH_RESULT_ERR;
	}

	if (flash_read != NULL) {
		return flash_read(flashchip, src_addr, dst_addr, size);
	}

	SpiFlashOpResult res;
	Cache_Read_Disable_2();
	res = SPIRead(src_addr, dst_addr, size);
	Cache_Read_Enable_2();
	return res;
}

SpiFlashOpResult
spi_flash_write(uint32_t des_addr, uint32_t *src_addr, uint32_t size)
{
	if (src_addr == NULL) {
		return SPI_FLASH_RESULT_ERR;
	}

	spi_flash_check_wr_protect();
	size = (size + 3) & ~3;

	pp_soft_wdt_feed();

	SpiFlashOpResult res;
	Cache_Read_Disable_2();
	res = SPIWrite(des_addr, src_addr, size);
	Cache_Read_Enable_2();
	return res;
}

SpiFlashOpResult
spi_flash_erase_sector(uint16_t sec)
{
	SpiFlashOpResult res;
	
	if (protect_flag && (!spi_flash_erase_sector_check(sec))) {
		return SPI_FLASH_RESULT_ERR;
	}

	spi_flash_check_wr_protect();
	pp_soft_wdt_feed();
	Cache_Read_Disable_2();
	res = SPIEraseSector(sec);
	Cache_Read_Enable_2();
	return res;
}

SpiFlashOpResult
spi_flash_enable_qmode(void)
{
	Cache_Read_Disable_2();
	SpiFlashOpResult res = Enable_QMode(flashchip);
	Wait_SPI_Idle(flashchip);
	Cache_Read_Enable_2();
	return res;
}

void
spi_flash_issi_enable_QIO_mode(void)
{
	/* TODO: re-add */
	unimplemented("spi_flash_issi_enable_QIO_mode");
}

bool
flash_gd25q32c_enable_QIO_mode(void)
{
	/* TODO: re-add */
	unimplemented("flash_gd25q32c_enable_QIO_mode");
	return false;
}

void
user_spi_flash_dio_to_qio_pre_init(void)
{
	uint32_t uVar1;
	bool bVar2;
	uint32_t flash_id = spi_flash_get_id();
	uint32_t mfg = flash_id & 0xff;
	SpiFlashOpResult res;
	
	if (mfg == 0x9d /* ISSI */ || mfg == 0xc2 /* MXIC */) {
		spi_flash_issi_enable_QIO_mode();
	}
	else if (flash_id == 0x1640c8 || flash_id == 0x1840c8) { /* GD25Q32C / GD25Q128 */
		res = flash_gd25q32c_enable_QIO_mode() ?  SPI_FLASH_RESULT_OK : SPI_FLASH_RESULT_ERR;
	}
	else {
		res = spi_flash_enable_qmode();
	}

	if (res == SPI_FLASH_RESULT_OK) {
		SPI0->CTRL &= ~0x01906000; /* clear QIO, QOUT, DIO, DOUT, FASTRD */
		SPI0->CTRL |=  0x01002000; /* set QIO and FASTRD */
	}
}

/*
         U Cache_Read_Disable
         U Cache_Read_Enable_New
         U Enable_QMode
         U flashchip
         U os_printf_plus
         U pp_soft_wdt_feed
         U protect_flag
         U SPIEraseSector
         U spi_flash_erase_sector_check
         U SPIRead
         U SPI_read_status
         U SPIWrite
         U SPI_write_enable
         U SPI_write_status
         U Wait_SPI_Idle
00000000 B flash_read
00000000 t flash_str$2102_14_7
00000004 T spi_flash_set_read_func
0000000c T Cache_Read_Disable_2
0000000c T Cache_Read_Enable_2
00000010 T spi_flash_read_status
00000014 T spi_flash_read
00000014 T spi_flash_write
00000018 T spi_flash_enable_qmode
0000001c T spi_flash_erase_sector
0000001c T spi_flash_get_id
00000028 W user_spi_flash_dio_to_qio_pre_init
0000002c T flash_gd25q32c_enable_QIO_mode
0000002c T spi_flash_write_status
00000030 t flash_str$2189_16_8
00000030 T spi_flash_check_wr_protect
00000048 T spi_flash_get_unique_id
00000058 T flash_gd25q32c_read_status
0000005c T flash_gd25q32c_write_status
00000060 t flash_str$2388_28_2
00000078 T spi_flash_issi_enable_QIO_mode
00000080 t flash_str$2396_28_8
00000090 t flash_str$2397_28_9
000000a0 t flash_str$2398_28_10
000000c0 t flash_str$2473_29_2
000000e0 t flash_str$2474_29_4
000000f0 t flash_str$2475_29_5
00000110 t flash_str$2476_29_6
*/

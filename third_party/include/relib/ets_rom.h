#ifndef ETS_ROM_H
#define ETS_ROM_H

#include "spi_flash.h"

extern SpiFlashChip *flashchip; // @0x3fffc714

void ets_delay_us(uint32_t us);
void ets_install_putc1(void (*p)(char c));
void *ets_memcpy(void *dest, const void *src, unsigned int nbyte);
void ets_bzero(void*, uint32_t);
void ets_printf(char*, ...);

uint32_t Wait_SPI_Idle(SpiFlashChip *chip);
SpiFlashOpResult SPIRead(uint32_t faddr, void *dst, size_t size);

#endif /* ETS_ROM_H */

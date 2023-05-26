#ifndef ESP8266_H
#define ESP8266_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define REG32(x) volatile uint32_t x

#if !defined(BIT)
#define BIT(x) (1 << x)
#endif

enum irq_num {
	SLC_IRQ = 1,
	SPI_IRQ = 2,
	RTC_IRQ = 3,
	GPIO_IRQ = 4,
	UART_IRQ = 5,
	CCOUNT_IRQ = 6,
	SW_IRQ = 7, /* Used by PendSV impl. in SDK */
	WDT_IRQ = 8,
	FRC1_IRQ = 9,
	FRC2_IRQ = 10,
};

#define SPI_CK_I_EDGE BIT(6)

struct spi_regs {
	REG32(CMD);          // 0x00
	REG32(ADDR);         // 0x04
	REG32(CTRL);         // 0x08
	REG32(CTRL1);        // 0x0c
	REG32(RD_STATUS);    // 0x10
	REG32(CTRL2);        // 0x14
	REG32(CLOCK);        // 0x18
	REG32(USER);         // 0x1c
	REG32(USER1);        // 0x20
	REG32(USER2);        // 0x24
	REG32(WR_STATUS);    // 0x28
	REG32(PIN);          // 0x2c
	REG32(SLAVE);        // 0x30
	REG32(SLAVE1);       // 0x34
	REG32(SLAVE2);       // 0x38
	REG32(SLAVE3);       // 0x3c
	REG32(W[16]);        // 0x40 - 0x7c
	REG32(unused[0x1c]); // 0x80
	REG32(EXT0);         // 0xf0
	REG32(EXT1);         // 0xf4
	REG32(EXT2);         // 0xf8
	REG32(EXT3);         // 0xfc
};

#define SPI0 ((struct spi_regs *) 0x60000200)
#define SPI1 ((struct spi_regs *) 0x60000100)

/*
//GPIO (0-15) PIN Control Bits
#define GPCWE 10 //WAKEUP_ENABLE (can be 1 only when INT_TYPE is high or low)
#define GPCI 7 //INT_TYPE (3bits) 0:disable,1:rising,2:falling,3:change,4:low,5:high
#define GPCD 2 //DRIVER 0:normal,1:open drain
#define GPCS 0 //SOURCE 0:GPIO_DATA,1:SigmaDelta
 */

#define GPIO_INT_TYPE_MASK (0x7 << 7)

struct gpio_regs {
	REG32(OUT);             // 0x000
	REG32(OUT_W1TS);        // 0x004
	REG32(OUT_W1TC);        // 0x008
	REG32(ENABLE);          // 0x00c
	REG32(ENABLE_W1TS);     // 0x010
	REG32(ENABLE_W1TC);     // 0x014
	REG32(IN);              // 0x018
	REG32(STATUS);          // 0x01c
	REG32(STATUS_W1TS);     // 0x020
	REG32(STATUS_W1TC);     // 0x024
	REG32(PINCTRL[16]);     // 0x028
	REG32(SIGMA_DELTA);     // 0x068
	REG32(RTC_CALIB_SYNC);  // 0x06c
	REG32(RTC_CALIB_VALUE); // 0x070
};

#define GPIO ((struct gpio_regs *) 0x60000300)

#ifdef __cplusplus
}
#endif

#endif /* ESP8266_H */

#ifndef ESP8266_H
#define ESP8266_H

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include <stdint.h>

#define OFFSET_OF(s, f) __builtin_offsetof(s, f)

#define REG32(x) volatile uint32_t x

#if !defined(BIT)
#define BIT(x) (1 << x)
#endif

#define REGBASE_LOAD(reg_base, reg) ({ \
	uint32_t retval; \
	void *base = (void*)(reg_base); \
	const uint32_t ofs = __builtin_offsetof(struct gpio_regs, reg); \
	__asm__ __volatile__( \
		"l32i %[retval], %[base], %[ofs]\n" \
		: [retval]"=a"(retval) \
		: [base]"p"(base), [ofs]"i"(ofs) \
	); \
	retval; \
})
#define REGBASE_STORE(reg_base, reg, reg_val) ({ \
	uint32_t val = reg_val; \
	void *base = (void*)(reg_base); \
	const uint32_t ofs = __builtin_offsetof(struct gpio_regs, reg); \
	__asm__ __volatile__( \
		"s32i %[val], %[base], %[ofs]\n" \
		: : [base]"p"(base), [ofs]"i"(ofs), [val]"a"(val) \
	); \
})
#define REG_LOAD(addr) REGBASE_LOAD(addr, 0)
#define REG_WRITE(addr, val) REGBASE_WRITE(addr, 0, val)

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

struct uart_regs {
	REG32(FIFO);      //  0x00
	REG32(INT_RAW);   //  0x04
	REG32(INT_ST);    //  0x08
	REG32(INT_ENA);   //  0x0C
	REG32(INT_CLR);   //  0x10
	REG32(CLKDIV);    //  0x14
	REG32(AUTOBAUD);  //  0x18
	REG32(STATUS);    //  0x1C
	REG32(CONF0);     //  0x20
	REG32(CONF1);     //  0x24
	REG32(LOWPULSE);  //  0x28
	REG32(HIGHPULSE); //  0x2C
	REG32(PULSE_NUM); //  0x30
	uint32_t res[(0x78-0x34)/4];
	REG32(DATE);      //  0x78
	REG32(ID);        //  0x7C
};

#define UART0 ((struct uart_regs *) 0x60000000)
#define UART1 ((struct uart_regs *) 0x60000f00)

#define SPI_CMD_FLASH_RDID   BIT(28)
#define SPI_CTRL_QIO_MODE    BIT(24)
#define SPI_CTRL_DIO_MODE    BIT(23)
#define SPI_CTRL_QOUT_MODE   BIT(20)
#define SPI_CTRL_ENABLE_AHB  BIT(17)
#define SPI_CTRL_DOUT_MODE   BIT(14)
#define SPI_CTRL_FASTRD_MODE BIT(13)
#define SPI_CK_I_EDGE        BIT(6)

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
	REG32(DATA[16]);     // 0x40 - 0x7c
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

struct frc_regs { /* "Frame Rate Control" timers */
	REG32(LOAD);  // 0x00
	REG32(COUNT); // 0x04
	/* bit8: read-only interrupt status
	 * bit7: enable
	 * bit6: auto-reload if 0
	 * bit3+bit2: prescaler: 0: /1  1: /16  2 or 3: /256
	 * bit0: edge irq if 0, level irq if 1
	 */
	REG32(CTRL);  // 0x08
	REG32(INT);   // 0x0c   /* write to clear */
	REG32(ALARM); // 0x10
};

#define FRC1 ((struct frc_regs *) 0x60000600) /* 23bit countdown timer */
#define FRC2 ((struct frc_regs *) 0x60000620) /* 32bit countup timer */

struct rtc_regs {
	REG32(SW_RESET);  // 0x000    Set bit31 to reset CPU
	REG32(SLP_VAL);   // 0x004    the target value of RTC_COUNTER for wakeup from light-sleep/deep-sleep
	REG32(SLP_CTL);   // 0x008    bit20/bit21 fiddled in rtc_enter_sleep/rtc_get_reset_reason
	REG32(XTAL_WAIT_TIME); // 0x00c   See pm_set_pll_xtal_wait_time
	REG32(RESET_CTL); // 0x010    used by rom_phy_reset_req
	union {           // 0x014
		REG32(STATE1);
		REG32(RESET_HW_CAUSE_REG);  // bits 0-3
	};
	union {           // 0x018
		REG32(STATE2);
		REG32(WAKEUP_HW_CAUSE_REG);  // bits 8-13
		// See pm_wakeup_opt
	};
	REG32(SLP_CNT_VAL); // 0x01c
	REG32(INT_ENA);   // 0x020
	REG32(INT_CLR);   // 0x024
	REG32(SLP_STATE);   // 0x028   used by pm_wait4wakeup
	uint32_t res2[(0x30-0x2c)/4];
	REG32(STORE[4]);      // 0x030
	REG32(R40);           // 0x040
	REG32(R44);           // 0x044
	REG32(R48);           // 0x048
	REG32(R4C);           // 0x04c
	REG32(R50);
	REG32(R54);
	REG32(R58);
	REG32(R5C);
	REG32(R60);
	REG32(R64);
	REG32(GPIO_OUT);      // 0x068
	uint32_t res4[(0x74-0x6c)/4];
	REG32(GPIO_ENABLE);   // 0x074
	REG32(R78);
	REG32(R7C);
	REG32(R80);
	REG32(R84);
	REG32(R88);
	REG32(GPIO_IN_DATA);  // 0x08c
	REG32(GPIO_CONF);     // 0x090
	REG32(R94);
	REG32(R98);
	REG32(R9C);
	REG32(XPD_DCDC_CONF); // 0x0a0
	REG32(RA4);
	// See pm_wakeup_opt
	REG32(RA8);
	REG32(RAC);
	REG32(RB0);
	REG32(RB4);
};
#define RTC ((struct rtc_regs *) 0x60000700)
static_assert(OFFSET_OF(struct rtc_regs, XPD_DCDC_CONF) == 0xa0, "reg_rtc offset mismatch");

struct wdt_regs {
	REG32(CTL);    // 0x000
	REG32(OP);     // 0x004
	REG32(OP_ND);  // 0x008
	uint32_t res[(0x14-0x0c)/4];
	REG32(RST);    // 0x014
};
#define WDT ((struct wdt_regs *) 0x60000900)

struct i2c_sar_regs {
	uint32_t reg000[(0x48-0x00)/4];
	REG32(reg048);    // 0x048
};
#define I2C_SAR ((struct i2c_sar_regs *) 0x60000d00)

struct rtc_mem {
	REG32(BACKUP[64]); // 0x000, 256 bytes */
	union {
		REG32(SYSTEM[64]); // 0x100, 256 bytes */
		struct {
			uint8_t padf4[0xf4];
			uint32_t SYSTEM_CHANCFG; // 0x1f4  bit16: flag, bit0..7: channel
		};
	};
	REG32(USER[128]);  // 0x200, 512 bytes */
};
#define RTCMEM ((struct rtc_mem *) 0x60001000)

static_assert(0x600011f4 == (uint32_t)&RTCMEM->SYSTEM_CHANCFG, "SYSTEM_CHANCFG addr mismatch");

struct iomux_regs {
	/* CONF bit 8: SPI0_CLK_EQU_SYS_CLK */
	/* CONF bit 9: SPI1_CLK_EQU_SYS_CLK */
	volatile uint32_t CONF;
	union {
		volatile uint32_t MUX[16];
		struct {
	REG32(GPIO12); /* 0: MTDI;    1: I2SI_DATA; 2: HSPIQ_MISO; 3: GPIO12; 4: UART0_DTR */
	REG32(GPIO13); /* 0: MTCK;    1: I2SI_BCK;  2: HSPID_MOSI; 3: GPIO13; 4: UART0_CTS */
	REG32(GPIO14); /* 0: MTMS;    1: I2SI_WS;   2: HSPI_CLK;   3: GPIO14; 4: UART0_DSR */
	REG32(GPIO15); /* 0: MTDO;    1: I2SO_BCK;  2: HSPI_CS0;   3: GPIO15; 4: U0RTS */
	REG32(GPIO3);  /* 0: U0RXD;   1: I2SO_DATA;                3: GPIO3;  4: CLK_XTAL_BK */
	REG32(GPIO1);  /* 0: U0TXD;   1: SPICS1;                   3: GPIO1;  4: CLK_RTC_BK */
	REG32(GPIO6);  /* 0: SDCLK;   1: SPICLK;                   3: GPIO6;  4: UART1_CTS */
	REG32(GPIO7);  /* 0: SDDATA0; 1: SPIQ_MISO;                3: GPIO7;  4: U1TXD */
	REG32(GPIO8);  /* 0: SDDATA1; 1: SPID_MOSI;                3: GPIO8;  4: U1RXD */
	REG32(GPIO9);  /* 0: SDDATA2; 1: SPIHD;                    3: GPIO9;  4: HSPIHD */
	REG32(GPIO10); /* 0: SDDATA3; 1: SPIWP;                    3: GPIO10; 4: HSPIWP */
	REG32(GPIO11); /* 0: SDCMD;   1: SPICS0;                   3: GPIO11; 4: U1RTS */
	REG32(GPIO0);  /* 0: GPIO0;   1: SPICS2;                              4: CLK_OUT */
	REG32(GPIO2);  /* 0: GPIO2;   1: I2SO_WS;   2: U1TXD_BK;              4: U0TXD_BK */
	REG32(GPIO4);  /* 0: GPIO4:   1: CLK_XTAL */
	REG32(GPIO5);  /* 0: GPIO5;   1: CLK_RTC */
		};
	};
};

#define IOMUX ((struct iomux_regs *) 0x60000800)

struct bb_regs {
	REG32(EVM);     // 0x000
	uint32_t res0004[(0x574-0x004)/4];
	REG32(reg574);  // 0x574
};
static_assert(OFFSET_OF(struct bb_regs, reg574) == 0x574, "reg574 offset mismatch");

#define BB ((struct bb_regs *) 0x60009800)

#define CACHE_READ_EN 0x100

struct dport_regs {
	volatile uint32_t NMI_INT_ENABLE;   // 0x000
	volatile uint32_t EDGE_INT_ENABLE;  // 0x004
	volatile uint32_t unknown_0x8;      // 0x008
	volatile uint32_t CACHE_FLASH_CTRL; // 0x00c
	volatile uint32_t unknown_0x10;     // 0x010
	volatile uint32_t CTL;              // 0x014
	/* void clockgate_watchdog(flg) { if(flg) 0x3FF00018 &= 0x77 else 0x3FF00018 |= 8; }
	   system_restart_core: _DAT_3ff00018 = >_DAT_3ff00018 & 0xffff8aff; */
	volatile uint32_t CLOCK_GATE;       // 0x018 /* probably */
	volatile uint32_t unknown_0x1c;     // 0x01c
	volatile uint32_t INT_STATUS;       // 0x020
	volatile uint32_t SPI_CACHE_CTL;    // 0x024
	/*
	#define IOSWAPU   0 //Swaps UART
	#define IOSWAPS   1 //Swaps SPI
	#define IOSWAPU0  2 //Swaps UART 0 pins (u0rxd <-> u0cts), (u0txd <-> u0rts)
	#define IOSWAPU1  3 //Swaps UART 1 pins (u1rxd <-> u1cts), (u1txd <-> u1rts)
	#define IOSWAPHS  5 //Sets HSPI with higher prio
	#define IOSWAP2HS 6 //Sets Two SPI Masters on HSPI
	#define IOSWAP2CS 7 //Sets Two SPI Masters on CSPI
	*/
	volatile uint32_t PERI_IO_SWAP;     // 0x028
	volatile uint32_t SLC_TX_DESC_DEBUG_REG;  // 0x02c
};

#define DPORT ((struct dport_regs *) 0x3ff00000)

struct wdev_base_regs {
	REG32(CTL0);     // 0x000
	REG32(RX_CTL);   // 0x004
	REG32(RX_HEAD);  // 0x008
	REG32(RX_AMPDU_HEAD);  // 0x00c
	REG32(CTL10);    // 0x010
	REG32(RX_DESC);  // 0x014
	REG32(reg018);   // 0x018
	REG32(reg01c);   // 0x01c
	REG32(reg020);   // 0x020
	REG32(reg024);   // 0x024
	REG32(reg028);   // 0x028
	REG32(reg02c);   // 0x02c
	REG32(reg030);   // 0x030
	REG32(reg034);   // 0x034
	REG32(reg038);   // 0x038  /* wDev_SnifferRxLDPC, wDev_SnifferRxHT40 */
	REG32(reg03c);   // 0x03c
	REG32(reg040);   // 0x040
	REG32(reg044);   // 0x044
	REG32(reg048);   // 0x048
	REG32(reg04c);   // 0x04c
	REG32(reg050);   // 0x050
	REG32(reg054);   // 0x054
	REG32(reg058);   // 0x058
	REG32(reg05c);   // 0x05c
	REG32(reg060);   // 0x060
	REG32(reg064);   // 0x064
	REG32(reg068);   // 0x068
	REG32(reg06c_recv_flag);   // 0x06c
	REG32(reg070);   // 0x070
	REG32(reg074);   // 0x074
	REG32(reg078);   // 0x078
	REG32(RX_MBLK_END);    // 0x07c
	REG32(RX_DESC_START); // 0x080
	REG32(RX_AMPDU_MBLK_END);   // 0x084
	REG32(RX_AMPDU_DESC_START); // 0x088
	uint32_t res08c[(0x164-0x08c)/4];
	REG32(BA_BITMAP_HI); // 0x164
	REG32(BA_BITMAP_LO); // 0x168
	REG32(REG16C);       // 0x16c
	REG32(REG170);       // 0x170
	REG32(BA_TIDSSN);    // 0x174
	REG32(REG178);       // 0x178
	uint32_t res17c[(0x1d4-0x17c)/4];
	REG32(REG1D4);       // 0x1d4
	uint32_t res1d8[(0x400-0x1d8)/4];
	REG32(AUTOACKRATE0);   // 0x400
	REG32(AUTOACKRATE1);   // 0x404
	REG32(AUTOACKRATE2);   // 0x408
};
static_assert(OFFSET_OF(struct wdev_base_regs, RX_DESC) == 0x14, "RX_DESC offset mismatch");
static_assert(OFFSET_OF(struct wdev_base_regs, REG178) == 0x178, "REG178 offset mismatch");
static_assert(OFFSET_OF(struct wdev_base_regs, REG1D4) == 0x1d4, "REG1D4 offset mismatch");
static_assert(OFFSET_OF(struct wdev_base_regs, AUTOACKRATE0) == 0x400, "AUTOACKRATE0 offset mismatch");

#define WDEV_BASE ((struct wdev_base_regs *) 0x3ff20000)

static_assert(0x3ff2006c == (uint32_t)&WDEV_BASE->reg06c_recv_flag, "0x3ff2006c addr mismatch");

struct wdev_crypto_regs {
	REG32(MAC0_CRYPTO_CONF);   // 0x000
	REG32(MAC1_CRYPTO_CONF);   // 0x004
	REG32(REG008);             // 0x008
	REG32(KEYENTRY_ENABLE);    // 0x00c
};
static_assert(OFFSET_OF(struct wdev_crypto_regs, KEYENTRY_ENABLE) == 0x00c, "KEYENTRY_ENABLE offset mismatch");

#define WDEV_CRYPTO ((struct wdev_crypto_regs *) 0x3ff20800)

struct wdev_regs {
	REG32(MICROS);  /* WDEV_COUNT_REG; microseconds, counting up */
	REG32(regc04);
	uint32_t res008[(0x014-0x008)/4];
	REG32(regc14);       // 0x014
	REG32(INT_ENA);      // 0x018
	REG32(INT_RAW);      // 0x01c /* wDev_SnifferRxHT40 (all pending) */
	REG32(INT_STATUS);   // 0x020 /* wDev_ProcessFiq (only enabled pending) */
	REG32(INT_ACK);      // 0x024
	REG32(MAC0_BSSID_LO); // 0x028
	REG32(MAC0_BSSID_HI); // 0x02c
	REG32(MAC1_BSSID_LO); // 0x030
	REG32(MAC1_BSSID_HI); // 0x034
	REG32(MAC0_BSSID_MASK_LO); // 0x038
	REG32(MAC0_BSSID_MASK_HI); // 0x03c
	REG32(MAC1_BSSID_MASK_LO); // 0x040
	REG32(MAC1_BSSID_MASK_HI); // 0x044
	REG32(MAC0_LO);      // 0x048 /* wDev_SetMacAddress */
	REG32(MAC0_HI);      // 0x04c
	REG32(MAC1_LO);      // 0x050
	REG32(MAC1_HI);      // 0x054
	REG32(MAC0_MASK_LO); // 0x058
	REG32(MAC0_MASK_HI); // 0x05c /* wDev_EnableUcRx */
	REG32(MAC1_MASK_LO); // 0x060
	REG32(MAC1_MASK_HI); // 0x064
	REG32(regc68);       // 0x068 /* wDev_Option_Init */
	REG32(regc6c);       // 0x06c /* wDev_Option_Init */
	REG32(CTL_C70);      // 0x070 /* wDev_SnifferRxHT40 */
	uint32_t res074[(0x084-0x074)/4];
	REG32(TXRX_STATE);   // 0x084
	REG32(regc88);       // 0x088 /* wDev_Option_Init */
	REG32(regc8c);       // 0x08c
	REG32(regc90);       // 0x090 /* wDev_Option_Init */
	REG32(regc94);       // 0x094 /* wDev_Option_Init */
	union {
		REG32(ACK_TYPE[8]);  // 0x098 /* wDev_SetFrameAckType */
	};
	uint32_t res0b8[(0x0c0-0x0b8)/4];
	REG32(TXQ_COLLISIONS); // 0x0c0 /* wDev_ClearTxqCollisions */
	uint32_t res0c4[(0x0dc-0x0c4)/4];
	REG32(TX_CFG0);      // 0x0dc /* Tx_Copy2Queue */
	REG32(TX_CFG1);      // 0x0e0 /* Tx_Copy2Queue */
	REG32(TX_CFG2);      // 0x0e4 /* Tx_Copy2Queue */
	REG32(TX_CFG3);      // 0x0e8 /* Tx_Copy2Queue */
	uint32_t res0ec[(0x118-0x0ec)/4];
	struct wdev_txqueue_entry {
		REG32(BACKOFF);  // 0x118 /* wDev_EnableTransmit */
		REG32(CFG0);     // 0x11c /* wDev_EnableTransmit, Tx_Copy2Queue */
		REG32(CFG1);     // 0x120 /* Tx_Copy2Queue */
		REG32(CFG2);     // 0x124 /* Tx_Copy2Queue */
		REG32(CFG3);     // 0x128 /* Tx_Copy2Queue */
		REG32(REG6);     // 0x12c
	} TXCFG[8]; /* 2*4 AC "Access Category"? */
	uint32_t res1cc[(0x204-0x1d8)/4];
	REG32(ACK_SNR);      // 0x204
	REG32(rege08);       // 0x208
};
static_assert(OFFSET_OF(struct wdev_regs, INT_ENA) == 0x18, "INT_ENA offset mismatch");
static_assert(OFFSET_OF(struct wdev_regs, MAC0_LO) == 0x48, "MAC0_LO offset mismatch");
static_assert(OFFSET_OF(struct wdev_regs, TXRX_STATE) == 0x84, "TXRX_STATE offset mismatch");
static_assert(OFFSET_OF(struct wdev_regs, ACK_TYPE) == 0x98, "ACK_TYPE offset mismatch");
static_assert(OFFSET_OF(struct wdev_regs, TX_CFG0) == 0xdc, "TX_CFG0 offset mismatch");
static_assert(OFFSET_OF(struct wdev_regs, ACK_SNR) == 0x204, "ACK_SNR offset mismatch");

#define WDEV ((struct wdev_regs *) 0x3ff20c00)
static_assert((uint32_t)(&WDEV->TX_CFG0)          == 0x3ff20cdc, "TX_CFG0 offset mismatch");
static_assert((uint32_t)(&WDEV->ACK_TYPE[4])      == 0x3ff20ca8, "ACK_TYPE[4] offset mismatch");
static_assert((uint32_t)(&WDEV->ACK_TYPE[6])      == 0x3ff20cb0, "ACK_TYPE[6] offset mismatch");
static_assert((uint32_t)(&WDEV->TXCFG[0].BACKOFF) == 0x3ff20d18, "TXCFG[0].BACKOFF offset mismatch");
static_assert((uint32_t)(&WDEV->TXCFG[7].BACKOFF) == 0x3ff20dc0, "TXCFG[7].BACKOFF offset mismatch");
static_assert((uint32_t)(&WDEV->rege08)           == 0x3ff20e08, "rege08 offset mismatch");

struct wdev_timer_regs {
	REG32(reg000);
	REG32(TSF0_TIME_LO);      // 0x004
	REG32(TSF0_TIME_HI);      // 0x008
	REG32(reg00c);
	REG32(reg010);
	REG32(SLEEP0_CONF);       // 0x014
	REG32(TSFSW0_LO);         // 0x018
	REG32(TSFSW0_HI);         // 0x01c
	uint32_t res020[(0x048-0x020)/4];
	REG32(MAC_TIM1_COUNT_LO); // 0x048  /* a.k.a. WDTVAL */
	REG32(MAC_TIM1_COUNT_HI); // 0x04c
	uint32_t res050;
	uint32_t res054;
	REG32(MAC_TIM1_CTL2);     // 0x058 /* maybe INT_ENA */
	REG32(MAC_TBTT_REG5C);    // 0x05c /* set to 0 in wDev_Reset_TBTT */
	REG32(MAC_TBTT_REG60);    // 0x060 /* set to 0 in wDev_Reset_TBTT */
	uint32_t res064[(0x098-0x064)/4];
	REG32(TSF0_TIMER_ENA);    // 0x098
	REG32(TSF0_TIMER_LO);     // 0x09c
	REG32(TSF0_TIMER_HI);     // 0x0a0
	uint32_t res0a4[(0x0c8-0x0a4)/4];
	REG32(MAC_TIM1_CTL);      // 0x0c8  /* a.k.a. WDTCTL */
	REG32(MAC_TIM1_ALARM_LO); // 0x0cc  /* a.k.a. WDTOVF */
	REG32(MAC_TIM1_ALARM_HI); // 0x0d0
	uint32_t res0d4[(0x0fc-0x0d4)/4];
	REG32(RXSTART_TIME);      // 0x0fc
};
static_assert(OFFSET_OF(struct wdev_timer_regs, MAC_TIM1_COUNT_LO) == 0x48, "MAC_TIM1_COUNT_LO offset mismatch");
static_assert(OFFSET_OF(struct wdev_timer_regs, MAC_TIM1_ALARM_LO) == 0xcc, "MAC_TIM1_ALARM_LO offset mismatch");
static_assert(OFFSET_OF(struct wdev_timer_regs, TSF0_TIMER_ENA) == 0x98, "TSF0_TIMER_ENA offset mismatch");
static_assert(OFFSET_OF(struct wdev_timer_regs, MAC_TIM1_CTL) == 0xc8, "MAC_TIM1_CTL offset mismatch");
static_assert(OFFSET_OF(struct wdev_timer_regs, RXSTART_TIME) == 0xfc, "RXSTART_TIME offset mismatch");

#define WDEV_TIMER ((struct wdev_timer_regs *) 0x3ff21000)

struct wdev_keyentry_regs {
	struct wdev_keyentry {
		REG32(ADDR_LO);
		REG32(ADDR_HI);
		REG32(DATA[8]);
	} E[24]; /* wDev_remove_KeyEntry_all_cnx goes over 24 entries */
};
static_assert(sizeof(struct wdev_keyentry) == 0x28, "struct wdev_keyentry size mismatch");

#define WDEV_KEYENTRY ((struct wdev_keyentry_regs *) 0x3ff21400)

#define EFUSE_D0_IS_ESP8285           BIT(4)
#define EFUSE_D1_VERSION_SHIFT        24
#define EFUSE_D1_VERSION_MASK         (0xf << 24)
#define EFUSE_D2_USE_TYPE2            BIT(15)
#define EFUSE_D2_IS_24PINS            BIT(14)
#define EFUSE_D2_4_GPIO_PAD           BIT(13)
#define EFUSE_D2_IS_48BITS_MAC        BIT(12)
#define EFUSE_D3_EMB_FLASH_SIZE_SHIFT 26
#define EFUSE_D3_EMB_FLASH_SIZE_MASK  (0x3 << 26)

struct efuse_regs {
	uint32_t DATA[4];
};

#define EFUSE ((struct efuse_regs *) 0x3ff00050)

#ifdef __cplusplus
}
#endif

#endif /* ESP8266_H */

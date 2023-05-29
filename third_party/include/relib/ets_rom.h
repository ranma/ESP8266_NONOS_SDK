#ifndef ETS_ROM_H
#define ETS_ROM_H

#include "c_types.h"
#include "spi_flash.h"

typedef enum {
	/* From lowest (0) to highest (32) */
	PRIO_USER0,
	PRIO_USER1,
	PRIO_USER2,
	PRIO_3,
	PRIO_4,
	PRIO_5,
	PRIO_6,
	PRIO_7,
	PRIO_8,
	PRIO_9,
	PRIO_10,
	PRIO_11,
	PRIO_12,
	PRIO_13,
	PRIO_14,
	PRIO_15,
	PRIO_16,
	PRIO_17,
	PRIO_18,
	PRIO_19,
	PRIO_PM=20,          /* pmTask in pm.o */
	PRIO_EVENT=21,       /* Event_task in user_interface.o */
	PRIO_22,
	PRIO_23,
	PRIO_24,
	PRIO_LWIP_THREAD=25, /* espconn_mbedtls.c */
	PRIO_ESPCONN=26,     /* espconn_tcp.c */
	PRIO_27,
	PRIO_IF0_STA=28,     /* lwip_if0_task in eagle_lwip_if.o */
	PRIO_IF1_AP=29,      /* lwip_if1_task in eagle_lwip_if.o */
	PRIO_30,
	PRIO_RTC_TIMER=31,   /* ets_rtc_timer_task in ets_timer.o */
	PRIO_PP=32,          /* ppTask in pp.o (probably "Packet Processing") */
} ETSPriority;

/*
  This structure is used in the argument list of "C" callable exception handlers.
  See `_xtos_set_exception_handler` details below.
*/
typedef struct {
  uint32_t epc;      // 0x00
  uint32_t ps;       // 0x04
  uint32_t sar;      // 0x08
  uint32_t unused;   // 0x0c
  union {
    struct {
      uint32_t a0;   // 0x10
      // note: no a1 here! (a1 is the stack pointer)
      uint32_t a2;   // 0x14
      uint32_t a3;   // 0x18
      uint32_t a4;   // 0x1c
      uint32_t a5;   // 0x20
      uint32_t a6;   // 0x24
      uint32_t a7;   // 0x28
      uint32_t a8;   // 0x2c
      uint32_t a9;   // 0x30
      uint32_t a10;  // 0x34
      uint32_t a11;  // 0x38
      uint32_t a12;  // 0x3c
      uint32_t a13;  // 0x40
      uint32_t a14;  // 0x44
      uint32_t a15;  // 0x48
    };
    uint32_t a_reg[15];
  };
  uint32_t cause;    // 0x4c
} xtos_exception_frame_t;

typedef void (*c_exception_handler_t)(xtos_exception_frame_t *ef, int cause);

extern SpiFlashChip *flashchip; // @0x3fffc714

int memcmp(const void *s1, const void *s2, size_t n);
int ets_memcmp(const void *s1, const void *s2, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
void *ets_memcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
void *ets_memset(void *s, int c, size_t n);
void ets_bzero(void*, uint32_t);
void bzero(void*, uint32_t);

void ets_delay_us(uint32_t us);
void ets_install_putc1(void (*p)(char c));
void ets_printf(char*, ...);

uint32_t rtc_get_reset_reason(void);

uint32_t Wait_SPI_Idle(SpiFlashChip *chip);
SpiFlashOpResult SPIRead(uint32_t faddr, void *dst, size_t size);

c_exception_handler_t _xtos_set_exception_handler(int cause, c_exception_handler_t fn);
/* 0 on success, 1 on error (queue full) */
int ets_post(ETSPriority prio, ETSSignal sig, ETSParam par);
void ets_task(ETSTask task, ETSPriority prio, ETSEvent *queue, uint8_t qlen);

#endif /* ETS_ROM_H */

#include <stdint.h>

#include "c_types.h"
#include "osapi.h"

#include "relib/esp8266.h"
#include "relib/xtensa.h"
#include "relib/ets_rom.h"

uint32_t ICACHE_FLASH_ATTR
bit_popcount(uint32_t n)
{
	return __builtin_popcount(n);
}

static char *putc_str;

static void ICACHE_FLASH_ATTR
ets_sprintf_putc(char c)
{
	*(putc_str++) = c;
}

int ICACHE_FLASH_ATTR
ets_sprintf(char *str, const char *format, ...)
{
	va_list ap;
	/* ROM ets_vprintf is not re-entrant */
	uint32_t saved = LOCK_IRQ_SAVE();
	putc_str = str;
	va_start(ap, format);
	int ret = ets_vprintf(ets_sprintf_putc, format, ap);
	va_end(ap);
	*(putc_str++) = 0;
	LOCK_IRQ_RESTORE(saved);
	return ret;
}

/*
         U ets_memcpy
         U ets_strlen
         U pvPortMalloc
         U system_get_free_heap_size
         U __udivsi3
         U __umodsi3
         U vPortFree
00000000 t mem_debug_file_34
00000008 T divide
00000044 T skip_atoi
0000007c t print_number
000001d0 T ets_vsnprintf
00000530 T ets_vsprintf
00000574 T ets_sprintf
00000658 T ets_strcat
00000678 T ets_strrchr
000006a4 T ets_strchr
000006bc T bit_popcount
000006f8 T ets_snprintf
 */

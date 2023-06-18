#ifndef LWIP2_ARCH_CC_H
#define LWIP2_ARCH_CC_H

#define MEMLEAK_DEBUG 1

#include <stdint.h>
#include "c_types.h"
#include "osapi.h"
#include "mem.h"

#define SYS_ARCH_DECL_PROTECT(lev) uint32_t lev
#define SYS_ARCH_PROTECT(lev) do { __asm__ __volatile__("rsil %0, 3\n" : "=a"(lev) ::); } while (0)
#define SYS_ARCH_UNPROTECT(lev) do {__asm__ __volatile__("wsr %0, PS\n" :: "a"(lev) :); } while (0)

#define LWIP_PLATFORM_DIAG(x) do { os_printf x; } while(0)
#define LWIP_PLATFORM_ASSERT(x) do { os_printf("Assertion \"%s\" failed at line %d in %s\n", x, __LINE__, __FILE__); *(int*)0=0; } while(0)

#define LWIP_PBUF_CUSTOM_DATA void *eb;

#define sys_jiffies sys_now
#define printf os_printf

#define mem_clib_malloc os_malloc
#define mem_clib_calloc os_calloc
#define mem_clib_free   os_free

#endif /* LWIP2_ARCH_CC_H */

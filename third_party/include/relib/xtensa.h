#ifndef XTENSA_H
#define XTENSA_H

#define LOCK_IRQ_SAVE() ({ \
	uint32_t ps; \
	__asm__ __volatile__("rsil %0, 3\n" : "=a"(ps) ::); \
	ps; \
})

#define LOCK_IRQ_RESTORE(c) do { \
	__asm__ __volatile__("wsr %0, PS\n" :: "a"(c) :); \
} while (0)

#define RSR(r) ({ \
	uint32_t v; \
	__asm__ __volatile__("rsr %0, " #r "\n" : "=a"(v) ::); \
	v; \
})

#define WSR(r, v) do { \
	__asm__ __volatile__("wsr %0, " #r "\n" :: "a"(v) :); \
} while (0)

#define INTCLEAR(c) do { \
	__asm__ __volatile__("wsr %0, INTCLEAR\n" :: "a"(c) :); \
} while (0)

#define UNALIGNED_L32(addr) ({ \
	uint32_t aligned = (addr) & ~3; \
	uint32_t retval, tmp; \
	__asm__ __volatile__( \
		"ssa8l  %[addr]\n" \
		"l32i %[ret], %[base], 0\n" \
		"l32i %[tmp], %[base], 4\n" \
		"src  %[ret], %[tmp],  %[ret]\n" \
	: [ret]"=&a"(retval), [tmp]"=&a"(tmp) \
	: [base]"a"(aligned), [addr]"a"(addr)); \
	retval; \
})

#define CONST_ONES_SHIFTR(shift) ({ \
	uint32_t retval; \
	__asm__ ( \
		"movi.n %[ret], -1\n" \
		"srli  %[ret], %[ret], " #shift "\n" \
	: [ret]"=a"(retval)); \
	retval; \
})

#endif /* XTENSA_H */

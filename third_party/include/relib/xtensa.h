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

#endif /* XTENSA_H */

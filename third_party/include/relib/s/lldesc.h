#ifndef RELIB_LLDESC_H
#define RELIB_LLDESC_H

struct lldesc;

struct lldesc {
	union {
		struct {
			uint32_t size:12;
			uint32_t length:12;
			uint32_t offset:5;
			uint32_t sosf:1;
			uint32_t eof:1;
			uint32_t owner:1;
			uint8_t *buf;
			struct lldesc *stqe_next;
		};
		volatile uint32_t u32[3];
	};
};
static_assert(sizeof(struct lldesc) == 12, "lldesc size mismatch");

typedef struct lldesc lldesc_st;

#endif /* RELIB_LLDESC_H */

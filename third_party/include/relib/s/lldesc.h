struct lldesc;

struct lldesc {
	uint32_t size:12;
	uint32_t length:12;
	uint32_t offset:5;
	uint32_t sosf:1;
	uint32_t eof:1;
	uint32_t owner:1;
	uint8_t *buf;
	struct lldesc * stqe_next;
};

struct esf_buf;
struct pbuf;

struct pbuf {
	/** next pbuf in singly linked pbuf chain */
	struct pbuf *next;
	/** pointer to the actual data in the buffer */
	void *payload;
	/** total length of this buffer and all next buffers in chain */
	uint16_t tot_len;
	/** length of this buffer */
	uint16_t len;
	/** pbuf_type as u8_t instead of enum to save space */
	uint8_t /*pbuf_type*/ type;
	/** misc flags */
	uint8_t flags;
	/** the reference count */
	uint16_t ref;
	/* add a pointer for esf_buf */
	struct esf_buf *eb;
};

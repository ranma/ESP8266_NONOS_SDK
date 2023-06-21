#ifndef RELIB_BOOT_HDR_PARAM_H
#define RELIB_BOOT_HDR_PARAM_H

typedef struct __attribute__((packed)) {
	union {
		struct boot_hdr_2 {
			char use_bin:4;
			char flag:4;
			uint8_t version;
			uint8_t pad[6];
		} boot_2;
		struct boot_hdr {
			char use_bin:2;
			char boot_status:1;
			char to_qio:1;
			char reverse:4;

			char version:5;
			char test_pass_flag:1;
			char test_start_flag:1;
			char enhance_boot_flag:1;

			char test_bin_addr[3];
			char user_bin_addr[3];
		} boot;
	};
} boot_hdr_param_st;
static_assert(sizeof(boot_hdr_param_st) == 8, "boot_hdr_param size mismatch");

#endif /* RELIB_BOOT_HDR_PARAM_H */

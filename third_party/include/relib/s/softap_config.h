#ifndef RELIB_SOFTAP_CONFIG_H
#define RELIB_SOFTAP_CONFIG_H

#include "relib/s/auth_mode.h"

struct softap_config {
	uint8_t ssid[32];
	uint8_t password[64];
	uint8_t ssid_len;
	uint8_t channel;
	auth_mode_t authmode;
	uint8_t ssid_hidden;
	uint8_t max_connection;
	uint16_t beacon_interval;
};
typedef struct softap_config softap_config_st;

static_assert(sizeof(softap_config_st) == 108, "softap_config_st size mismatch");

#endif /* RELIB_SOFTAP_CONFIG_H */

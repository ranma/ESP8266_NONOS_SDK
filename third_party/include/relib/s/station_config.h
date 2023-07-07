#ifndef RELIB_STATION_CONFIG_H
#define RELIB_STATION_CONFIG_H

#include "relib/s/auth_mode.h"

struct wifi_fast_scan_threshold {
	uint8_t rssi;
	auth_mode_t authmode;
};

struct station_config {
	uint8_t ssid[32];
	uint8_t password[64];
	uint8_t channel;
	bool bssid_set;
	uint8_t bssid[6];
	struct wifi_fast_scan_threshold threshold;
	bool open_and_wep_mode_disable;
	bool all_channel_scan;
};
typedef struct station_config station_config_st;

static_assert(sizeof(station_config_st) == 116, "station_config_st size mismatch");

#endif /* RELIB_STATION_CONFIG_H */

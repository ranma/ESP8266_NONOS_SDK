#ifndef RELIB_HOSTAPD_DATA_H
#define RELIB_HOSTAPD_DATA_H

struct hostapd_config;
struct hostapd_bss_config;
struct wpa_authenticator;

struct hostapd_data {
	struct hostapd_config * iconf;
	struct hostapd_bss_config * conf;
	int interface_added;
	uint8_t own_addr[6];
	int num_sta;
	struct wpa_authenticator * wpa_auth;
};
typedef struct hostapd_data hostapd_data_st;

static_assert(sizeof(hostapd_data_st) == 28, "hostapd_data_st size mismatch");

#endif /* RELIB_HOSTAPD_DATA_H */

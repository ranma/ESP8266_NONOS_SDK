#ifndef RELIB_WPA_AUTHENTICATOR_H
#define RELIB_WPA_AUTHENTICATOR_H

struct wpa_auth_config {
	int wpa;
	int wpa_key_mgmt;
	int wpa_pairwise;
	int wpa_group;
	int wpa_group_rekey;
	int wpa_strict_rekey;
	int wpa_gmk_rekey;
	int wpa_ptk_rekey;
	int rsn_pairwise;
	int rsn_preauth;
	int eapol_version;
	int peerkey;
	int wmm_enabled;
	int wmm_uapsd;
	int disable_pmksa_caching;
	int okc;
	int tx_status;
	int disable_gtk;
	int ap_mlme;
};
typedef struct wpa_auth_config wpa_auth_config_st;
static_assert(sizeof(wpa_auth_config_st) == 76, "wpa_auth_config_st size mismatch");

struct wpa_authenticator {
	struct wpa_group * group;
	struct wpa_auth_config conf;
	uint8_t * wpa_ie;
	uint32_t wpa_ie_len;
	uint8_t addr[6];
};
typedef struct wpa_authenticator wpa_authenticator_st;
static_assert(sizeof(wpa_authenticator_st) == 96, "wpa_authenticator_st size mismatch");

#endif /* RELIB_WPA_AUTHENTICATOR_H */

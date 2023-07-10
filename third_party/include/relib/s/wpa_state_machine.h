#ifndef RELIB_WPA_STATE_MACHINE_H
#define RELIB_WPA_STATE_MACHINE_H

struct wpa_authenticator;
struct wpa_group;

typedef enum Boolean {
	WPA_FALSE=0,
	WPA_TRUE=1
} Boolean_t;

struct wpa_state_machine {
	struct wpa_authenticator *wpa_auth;
	struct wpa_group *group;
	uint8_t addr[6];
	enum {
		WPA_PTK_INITIALIZE, WPA_PTK_DISCONNECT, WPA_PTK_DISCONNECTED,
		WPA_PTK_AUTHENTICATION, WPA_PTK_AUTHENTICATION2,
		WPA_PTK_INITPMK, WPA_PTK_INITPSK, WPA_PTK_PTKSTART,
		WPA_PTK_PTKCALCNEGOTIATING, WPA_PTK_PTKCALCNEGOTIATING2,
		WPA_PTK_PTKINITNEGOTIATING, WPA_PTK_PTKINITDONE
	} wpa_ptk_state;
	enum {
		WPA_PTK_GROUP_IDLE = 0,
		WPA_PTK_GROUP_REKEYNEGOTIATING,
		WPA_PTK_GROUP_REKEYESTABLISHED,
		WPA_PTK_GROUP_KEYERROR
	} wpa_ptk_group_state;
	enum Boolean Init;
	enum Boolean DeauthenticationRequest;
	enum Boolean AuthenticationRequest;
	enum Boolean ReAuthenticationRequest;
	enum Boolean Disconnect;
	int TimeoutCtr;
	int GTimeoutCtr;
	enum Boolean TimeoutEvt;
	enum Boolean EAPOLKeyReceived;
	enum Boolean EAPOLKeyPairwise;
	enum Boolean EAPOLKeyRequest;
	enum Boolean MICVerified;
	enum Boolean GUpdateStationKeys;
	uint8_t ANonce[32];
	uint8_t SNonce[32];
	uint8_t PMK[32];
	struct wpa_ptk PTK;
	enum Boolean PTK_valid;
	enum Boolean pairwise_set;
	int keycount;
	enum Boolean Pair;
	struct wpa_key_replay_counter key_replay[4];
	struct wpa_key_replay_counter prev_key_replay[4];
	enum Boolean PInitAKeys;
	enum Boolean PTKRequest;
	enum Boolean has_GTK;
	enum Boolean PtkGroupInit;
	uint8_t *last_rx_eapol_key;
	uint32_t last_rx_eapol_key_len;
	uint32_t changed:1;
	uint32_t in_step_loop:1;
	uint32_t pending_deinit:1;
	uint32_t started:1;
	uint32_t mgmt_frame_prot:1;
	uint32_t rx_eapol_key_secure:1;
	uint32_t update_snonce:1;
	uint32_t is_wnmsleep:1;
	uint8_t req_replay_counter[8];
	int req_replay_counter_used;
	uint8_t *wpa_ie;
	uint32_t wpa_ie_len;
	enum {
		WPA_VERSION_NO_WPA = 0 /* WPA not used */,
		WPA_VERSION_WPA = 1 /* WPA / IEEE 802.11i/D3.0 */,
		WPA_VERSION_WPA2 = 2 /* WPA2 / IEEE 802.11i */
	} wpa;
	int pairwise;
	int wpa_key_mgmt;
	int pending_1_of_4_timeout;
	uint32_t index;
};

#endif /* RELIB_WPA_STATE_MACHINE_H */

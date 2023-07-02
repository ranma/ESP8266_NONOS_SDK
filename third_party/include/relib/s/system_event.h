#ifndef RELIB_SYSTEM_EVENT_H
#define RELIB_SYSTEM_EVENT_H

#include "relib/s/ip_addr.h"

typedef struct {
	uint8_t ssid[32];
	uint8_t ssid_len;
	uint8_t bssid[6];
	uint8_t channel;
} Event_StaMode_Connected_st;

typedef struct {
	uint8_t ssid[32];
	uint8_t ssid_len;
	uint8_t bssid[6];
	uint8_t reason;
} Event_StaMode_Disconnected_st;

typedef struct {
	uint8_t old_mode;
	uint8_t new_mode;
} Event_StaMode_AuthMode_Change_st;

typedef struct {
	ip_addr_st ip;
	ip_addr_st mask;
	ip_addr_st gw;
} Event_StaMode_Got_IP_st;

typedef struct {
	uint8_t mac[6];
	uint8_t aid;
} Event_SoftAPMode_StaConnected_st;

typedef struct {
	uint8_t mac[6];
	ip_addr_st ip;
	uint8_t aid;
} Event_SoftAPMode_Distribute_Sta_IP_st;

typedef struct {
	uint8_t mac[6];
	uint8_t aid;
} Event_SoftAPMode_StaDisconnected_st;

typedef struct {
	int rssi;
	uint8_t mac[6];
} Event_SoftAPMode_ProbeReqRecved_st;

typedef struct {
	uint8_t old_opmode;
	uint8_t new_opmode;
} Event_OpMode_Change_st;

typedef union {
	Event_StaMode_Connected_st			connected;
	Event_StaMode_Disconnected_st			disconnected;
	Event_StaMode_AuthMode_Change_st		auth_change;
	Event_StaMode_Got_IP_st				got_ip;
	Event_SoftAPMode_StaConnected_st		sta_connected;
	Event_SoftAPMode_Distribute_Sta_IP_st		distribute_sta_ip;
	Event_SoftAPMode_StaDisconnected_st		sta_disconnected;
	Event_SoftAPMode_ProbeReqRecved_st		ap_probereqrecved;
	Event_OpMode_Change_st				opmode_changed;
} Event_Info_u;

enum system_event_type {
	EVENT_GOT_IP=3,
	EVENT_OPMODE_CHANGED=8,
	EVENT_DISTRIBUTE_STA_IP=9,
};

typedef struct _esp_event {
	enum system_event_type event;
	Event_Info_u event_info;
} System_Event_st;

static_assert(sizeof(System_Event_st) == 0x2c, "System_Event_st size mismatch");

#endif /* RELIB_SYSTEM_EVENT_H */

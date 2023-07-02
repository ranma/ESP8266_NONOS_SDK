#ifndef RELIB_AUTH_MODE_H
#define RELIB_AUTH_MODE_H

enum auth_mode {
	AUTH_OPEN=0,
	AUTH_WEP=1,
	AUTH_WPA_PSK=2,
	AUTH_WPA2_PSK=3,
	AUTH_WPA_WPA2_PSK=4,
	AUTH_WPA2_ENTERPRISE=5,
	AUTH_MAX=6
};
typedef enum auth_mode auth_mode_t;

#endif /* RELIB_AUTH_MODE_H */

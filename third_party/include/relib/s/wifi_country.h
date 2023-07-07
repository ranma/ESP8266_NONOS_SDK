#ifndef RELIB_WIFI_COUNTRY_H
#define RELIB_WIFI_COUNTRY_H

struct wifi_country {
	char cc[3];
	uint8_t schan;
	uint8_t nchan;
	uint8_t policy;
};
typedef struct wifi_country wifi_country_st;

static_assert(sizeof(wifi_country_st) == 6, "wifi_country_st size mismatch");

#endif /* RELIB_WIFI_COUNTRY_H */

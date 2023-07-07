#include <assert.h>
#include <stdint.h>

#include "c_types.h"
#include "mem.h"
#include "osapi.h"
#include "spi_flash.h"
#include "gpio.h"

#include "relib/relib.h"
#include "relib/esp8266.h"
#include "relib/xtensa.h"
#include "relib/ets_rom.h"

#include "relib/s/chm_ctx.h"
#include "relib/s/cnx_mgr.h"
#include "relib/s/ieee80211com.h"
#include "relib/s/ieee80211_bss.h"
#include "relib/s/ieee80211_conn.h"
#include "relib/s/wifi_country.h"

#if 1
#define DPRINTF(fmt, ...) do { \
	ets_printf("[@%p] " fmt, __builtin_return_address(0), ##__VA_ARGS__); \
	while ((UART0->STATUS & 0xff0000) != 0); \
} while (0)
#else
#define DPRINTF(fmt, ...)
#endif

extern ieee80211com_st g_ic; /* ieee80211.c */

wifi_country_st* ICACHE_FLASH_ATTR
ieee80211_regdomain_get_country(void)
{
	wifi_country_st *country;
	ieee80211_bss_st *bss;

	country = &g_ic.ic_country;
	if ((((g_ic.ic_country.policy == '\0') && (g_ic.ic_if0_conn != NULL)) &&
			((g_ic.ic_if0_conn)->ni_mlme_state == IEEE80211_S_RUN)) &&
		 ((bss = (g_ic.ic_if0_conn)->ni_bss, bss != NULL &&
			((bss->ni_country).schan != '\0')))) {
		country = &bss->ni_country;
	}
	return country;
}


int ICACHE_FLASH_ATTR
ieee80211_regdomain_min_chan(void)
{
	wifi_country_st *country = ieee80211_regdomain_get_country();
	return country->schan;
}

int ICACHE_FLASH_ATTR
ieee80211_regdomain_max_chan(void)
{
	wifi_country_st *country = ieee80211_regdomain_get_country();
	return country->schan + country->nchan - 1;
}

bool ICACHE_FLASH_ATTR
ieee80211_regdomain_chan_in_range(uint8_t chan)
{
	if ((ieee80211_regdomain_max_chan() < chan) || (chan < ieee80211_regdomain_min_chan())) {
		return false;
	}
	return true;
}

bool ICACHE_FLASH_ATTR
ieee80211_regdomain_is_active_scan(uint8_t chan)
{
	int max_chan = ieee80211_regdomain_max_chan();
	int active_max = max_chan;
	ieee80211_conn_st *conn = g_ic.ic_if0_conn;

	if (((g_ic.ic_country.policy == 0) && (conn != NULL)) && (conn->ni_mlme_state != IEEE80211_S_RUN)) {
		active_max = 11;
		if (max_chan < 12) {
			active_max = max_chan;
		}
	}
	if ((active_max < chan) || (!ieee80211_regdomain_chan_in_range(chan))) {
		return false;
	}
	return true;
}

void ICACHE_FLASH_ATTR
ieee80211_regdomain_attach(ieee80211com_st *ic)
{
	DPRINTF("ieee80211_regdomain_attach\n");
	(ic->ic_country).policy = 0;
	(ic->ic_country).cc[0] = 'C';
	(ic->ic_country).cc[1] = 'N';
	(ic->ic_country).cc[2] = 0;
	(ic->ic_country).schan = 1;
	(ic->ic_country).nchan = 13;
}

uint8_t* ICACHE_FLASH_ATTR
ieee80211_add_countryie(uint8_t *frm, ieee80211com_st *ic)
{
	wifi_country_st *country = ieee80211_regdomain_get_country();
	frm[0] = 10;
	frm[1] = 6;
	frm[2] = country->cc[0];
	frm[3] = country->cc[1];
	frm[4] = country->cc[2];
	frm[5] = country->schan;
	frm[6] = country->nchan;
	frm[7] = 0x14;
	return frm + 8;
}

bool ICACHE_FLASH_ATTR
ieee80211_regdomain_update(ieee80211_bss_st *bss,uint8_t *frm)
{
	if (g_ic.ic_country.policy == 1) {
		return true;
	}
	if (bss == NULL || frm == NULL) {
		return false;
	}

	wifi_country_st *country = &bss->ni_country;
	memset(country,0,6);
	if (frm[1] != 6) {
		return false;
	}

	wifi_country_st new_country = {
		.cc[0] = frm[2],
		.cc[1] = frm[3],
		.cc[2] = frm[4],
		.schan = frm[5],
		.nchan = frm[6],
	};
	if (memcmp(country, &new_country, sizeof(*country)) != 0) {
		DPRINTF("ieee80211_regdomain_update(%c%c s=%d n=%d)\n",
			frm[2], frm[3], frm[5], frm[6]);
		*country = new_country;
	}
	return true;
}

/*
         U __divsi3
         U ets_memcmp
         U ets_memcpy
         U ets_memset
         U g_ic
00000000 T ieee80211_regdomain_attach
00000000 t mem_debug_file_91
00000004 T ieee80211_add_countryie
00000004 T ieee80211_regdomain_max_chan
00000004 T ieee80211_regdomain_min_chan
00000008 T ieee80211_regdomain_chan_in_range
00000008 T ieee80211_regdomain_get_country
00000010 T ieee80211_regdomain_is_active_scan
00000018 T ieee80211_regdomain_update
*/

#ifndef RELIB_IEEE80211_CONN_H
#define RELIB_IEEE80211_CONN_H

struct netif;

typedef enum ieee80211_opmode {
	IEEE80211_M_STA=0,
	IEEE80211_M_HOSTAP=1
} ieee80211_opmode;

struct ieee80211_conn {
	/* shallow struct def to avoid pulling in all the other struct members */
	struct netif *ni_ifp;
	uint8_t pad1[200 - 4];
	enum ieee80211_opmode opmode; /* member at offset 200 */
	uint8_t pad2[672 - 204];
};

typedef struct ieee80211_conn ieee80211_conn_st;

static_assert(sizeof(ieee80211_conn_st) == 672, "ieee80211_conn size mismatch");
static_assert(OFFSET_OF(ieee80211_conn_st, opmode) == 200, "opmode offset mismatch");

#endif /* RELIB_IEEE80211_CONN_H */

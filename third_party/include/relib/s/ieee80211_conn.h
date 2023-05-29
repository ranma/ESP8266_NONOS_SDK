#define OFFSET_OF(s, f) __builtin_offsetof(s, f)

struct netif;

typedef enum ieee80211_opmode {
	IEEE80211_M_STA=0,
	IEEE80211_M_HOSTAP=1
} ieee80211_opmode;

typedef struct {
	/* shallow struct def to avoid pulling in all the other struct members */
	struct netif *ni_ifp;
	uint8_t pad1[200 - 4];
	enum ieee80211_opmode opmode; /* member at offset 200 */
	uint8_t pad2[672 - 204];
} ieee80211_conn_st;

static_assert(sizeof(ieee80211_conn_st) == 672, "ieee80211_conn size mismatch");
static_assert(OFFSET_OF(ieee80211_conn_st, opmode) == 200, "opmode offset mismatch");

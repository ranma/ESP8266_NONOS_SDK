#ifndef RELIB_EAGLE_LWIP_IF_H
#define RELIB_EAGLE_LWIP_IF_H

#include "relib/s/netif.h"
#include "relib/s/ip_info.h"
#include "relib/s/ieee80211_conn.h"

struct netif* eagle_lwip_getif(int i);
void eagle_lwip_if_free(ieee80211_conn_st *conn);
struct netif* eagle_lwip_if_alloc(ieee80211_conn_st *conn, uint8_t *macaddr, ip_info_st *ipinfo);

#endif /* RELIB_EAGLE_LWIP_IF_H */

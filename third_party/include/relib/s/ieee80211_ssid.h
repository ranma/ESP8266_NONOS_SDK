#ifndef RELIB_IEEE80211_SSID_H
#define RELIB_IEEE80211_SSID_H

struct ieee80211_ssid {
    int len;
    uint8_t ssid[32];
};

#endif /* RELIB_IEEE80211_SSID_H */

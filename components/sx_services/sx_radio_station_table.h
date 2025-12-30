#pragma once

#include <strings.h>

// Vietnamese VOV stations (AAC).  Simple key → url + display name.
// Only fields required by service: key + url + human title.

typedef struct {
    const char *key;      // e.g. "VOV1"
    const char *url;      // HTTPS AAC stream
    const char *title;    // Display title
} sx_radio_station_t;

static const sx_radio_station_t g_vn_vov_stations[] = {
    {"VOV1",        "https://stream.vovmedia.vn/vov-1",      "VOV 1 – Thời sự"},
    {"VOV2",        "https://stream.vovmedia.vn/vov-2",      "VOV 2 – Văn hoá & Giáo dục"},
    {"VOV3",        "https://stream.vovmedia.vn/vov-3",      "VOV 3 – Âm nhạc & Giải trí"},
    {"VOV5",        "https://stream.vovmedia.vn/vov5",       "VOV 5 – Đối ngoại"},
    {"VOV_GT_HN",   "https://stream.vovmedia.vn/vovgt-hn",   "VOV GT Hà Nội"},
    {"VOV_GT_HCM",  "https://stream.vovmedia.vn/vovgt-hcm",  "VOV GT TP.HCM"},
    {"VOV_MEKONG",  "https://stream.vovmedia.vn/vovmekong",  "VOV Mekong"},
    {"VOV4_MT",     "https://stream.vovmedia.vn/vov4mt",     "VOV4 Miền Trung"},
    {"VOV4_TB",     "https://stream.vovmedia.vn/vov4tb",     "VOV4 Tây Bắc"},
    {"VOV4_DB",     "https://stream.vovmedia.vn/vov4db",     "VOV4 Đông Bắc"},
    {"VOV4_TN",     "https://stream.vovmedia.vn/vov4tn",     "VOV4 Tây Nguyên"},
    {"VOV4_DBSCL",  "https://stream.vovmedia.vn/vov4dbscl",  "VOV4 ĐBSCL"},
    {"VOV4_HCM",    "https://stream.vovmedia.vn/vov4hcm",    "VOV4 TP.HCM"},
    {"VOV5_EN",     "https://stream.vovmedia.vn/vov247",     "VOV5 English 24/7"},
};

static inline const sx_radio_station_t *sx_radio_lookup_station(const char *key) {
    if (!key) return NULL;
    for (size_t i = 0; i < sizeof(g_vn_vov_stations)/sizeof(g_vn_vov_stations[0]); ++i) {
        if (strcasecmp(key, g_vn_vov_stations[i].key) == 0) return &g_vn_vov_stations[i];
    }
    return NULL;
}


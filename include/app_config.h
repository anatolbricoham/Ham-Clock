#pragma once

#if __has_include("app_config.local.h")
#include "app_config.local.h"
#endif

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif
#ifndef MAIDENHEAD_LOCATOR
#define MAIDENHEAD_LOCATOR "AA00"
#endif
#ifndef PROPAGATION_JSON_URL
#define PROPAGATION_JSON_URL ""
#endif
#ifndef DX_SPOTS_URL
#define DX_SPOTS_URL ""
#endif
#ifndef APP_SETTINGS_NAMESPACE
#define APP_SETTINGS_NAMESPACE "cyd-hamclock"
#endif
#ifndef TIMEZONE_DEFAULT
#define TIMEZONE_DEFAULT "UTC0"
#endif
#ifndef TIMEZONE_LABEL_DEFAULT
#define TIMEZONE_LABEL_DEFAULT "UTC"
#endif
#ifndef CALLSIGN_DEFAULT
#define CALLSIGN_DEFAULT ""
#endif
#ifndef OPENWEBRX_URL_DEFAULT
#define OPENWEBRX_URL_DEFAULT ""
#endif
#ifndef DMR_HOTSPOT_URL_DEFAULT
#define DMR_HOTSPOT_URL_DEFAULT ""
#endif
#ifndef N2YO_API_KEY_DEFAULT
#define N2YO_API_KEY_DEFAULT ""
#endif
#ifndef APRS_RADIUS_KM_DEFAULT
#define APRS_RADIUS_KM_DEFAULT 100
#endif
#ifndef APRSFI_API_KEY_DEFAULT
#define APRSFI_API_KEY_DEFAULT ""
#endif

// Uncomment display overrides here only when they should be shared by every
// build from this checkout. Device-specific orientation belongs in the local
// configuration file or web settings.
// #define ROTATE90_DEFAULT true
// #define SWAP_RED_BLUE_DEFAULT true
// #define FLIP180_DEFAULT true
// #define MIRROR_DEFAULT true
// #define INVERT_COLOURS_DEFAULT true
// #define SWAP_TOUCH_NAV_DEFAULT true

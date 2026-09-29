#pragma once

// Copy to include/app_config.local.h for per-device overrides.
// The local file is ignored by Git; keep credentials and API keys there.

#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#define MAIDENHEAD_LOCATOR "AA00"
#define APP_SETTINGS_NAMESPACE "cyd-hamclock"
#define TIMEZONE_DEFAULT "UTC0"
#define TIMEZONE_LABEL_DEFAULT "UTC"
#define CALLSIGN_DEFAULT ""
#define OPENWEBRX_URL_DEFAULT ""
#define DMR_HOTSPOT_URL_DEFAULT ""
#define N2YO_API_KEY_DEFAULT ""
#define APRS_RADIUS_KM_DEFAULT 100
#define APRSFI_API_KEY_DEFAULT ""

#define PROPAGATION_JSON_URL ""
#define DX_SPOTS_URL ""

// Optional display defaults for boards that always need the same orientation
// fix, applied on first boot or after a factory reset. These can otherwise be
// set per-device from the web settings page instead. Uncomment to override.
// #define ROTATE90_DEFAULT true
// #define SWAP_RED_BLUE_DEFAULT true
// #define FLIP180_DEFAULT true
// #define MIRROR_DEFAULT true
// #define INVERT_COLOURS_DEFAULT true
// #define SWAP_TOUCH_NAV_DEFAULT true

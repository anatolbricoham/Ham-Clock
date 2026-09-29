# Settings

Files: `src/settings.h`, `src/settings.cpp`, `include/app_config.h`, `include/app_config.example.h`, `include/app_config.local.h`

## Public API

```cpp
void settingsBegin();                        // open NVS, load, normalise
const AppSettings& getSettings();            // current settings (read-only)
void saveSettings(const AppSettings& s);     // normalise + write every key
bool hasWifiCredentials();                   // any SSID saved?
void factoryResetSettings();                 // preferences.clear() + settingsBegin()
```

`AppSettings` (in `settings.h`) is a plain struct holding every user setting. Modules read it through `getSettings()` on every refresh, so changes saved from the web page take effect without a reboot.

## Where values come from

1. **Compile-time defaults** – `include/app_config.h` defines each macro with `#ifndef` fallbacks, after first including `include/app_config.local.h` if it exists (`__has_include`). Put per-device secrets in `app_config.local.h`; it is git-ignored.
2. **Saved values** – ESP32 Preferences (NVS) in the namespace `APP_SETTINGS_NAMESPACE`. A saved value always wins over the compile-time default.
3. **Normalisation** – `normalizeSettings()` trims and truncates strings, upper-cases callsign and locator, validates the Maidenhead locator (falls back to `MAIDENHEAD_LOCATOR`), and clamps every number to its valid range. It runs after loading and before saving.

### Compile-time macros (`app_config.local.h`)

| Macro | Fallback | Used for |
| --- | --- | --- |
| `WIFI_SSID` / `WIFI_PASSWORD` | `""` | Wi-Fi slot 1 when nothing is saved (ignored if empty or starting with `your-`) |
| `MAIDENHEAD_LOCATOR` | `"AA00"` | Default and fallback locator |
| `APP_SETTINGS_NAMESPACE` | `"cyd-hamclock"` | NVS namespace (max 15 chars) |
| `TIMEZONE_DEFAULT` | `"UTC0"` | POSIX TZ rule |
| `TIMEZONE_LABEL_DEFAULT` | `"UTC"` | Label shown next to local time |
| `CALLSIGN_DEFAULT` | `""` | Station callsign |
| `OPENWEBRX_URL_DEFAULT` | `""` | OpenWebRX base URL |
| `DMR_HOTSPOT_URL_DEFAULT` | `""` | DMR hotspot base URL |
| `N2YO_API_KEY_DEFAULT` | `""` | N2YO key (ISS passes) |
| `APRS_RADIUS_KM_DEFAULT` | `100` | APRS nearby radius |
| `APRSFI_API_KEY_DEFAULT` | `""` | APRS.fi key |
| `PROPAGATION_JSON_URL` | `""` | Propagation JSON proxy URL |
| `DX_SPOTS_URL` | `""` | DX JSON URL (empty → IZ3MEZ feed) |
| `ROTATE90_DEFAULT`, `FLIP180_DEFAULT`, `MIRROR_DEFAULT`, `INVERT_COLOURS_DEFAULT`, `SWAP_RED_BLUE_DEFAULT`, `SWAP_TOUCH_NAV_DEFAULT` | `false` | Display defaults applied on first boot / after factory reset |

> **Important – NVS namespace.** Earlier releases stored settings under the fixed namespace `"hamclock"`. The namespace is now `APP_SETTINGS_NAMESPACE` (default `"cyd-hamclock"`). A device upgraded from an older build starts with empty settings (Wi-Fi, locator, etc.) unless `APP_SETTINGS_NAMESPACE` is set back to `"hamclock"`.

> **Important – display defaults.** Older `app_config.h` set per-panel orientation defaults automatically (ILI9341: rotate + mirror + invert; ST7796: rotate + flip + swap R/B). They are now commented out, so a fresh or factory-reset board may start rotated/mirrored until you tick the options in the web page or define them in `app_config.local.h`. See the [configuration manual](../configuration-manual.md#display-orientation-and-colours).

## Settings table

"Portal field" is the `name` attribute in the web form (`setup_portal.cpp`).

### Station and Wi-Fi

| Field | NVS key | Type | Default | Range / rules | Portal field |
| --- | --- | --- | --- | --- | --- |
| `callsign` | `callsign` | String | `CALLSIGN_DEFAULT` | ≤16 chars, upper-cased | `callsign` |
| `wifiNetworks[i].ssid` (i = 0–4) | `wifi<i>s` | String | slot 0: legacy `ssid` key, then `WIFI_SSID` | ≤64 chars, not trimmed | `ssid<i>` |
| `wifiNetworks[i].password` | `wifi<i>p` | String | legacy `pass`, then `WIFI_PASSWORD` | ≤64; blank field keeps the saved password; cleared if SSID empty | `pass<i>`, `clearpass<i>` |
| `keepHotspotOn` | `apalwayson` | bool | false | – | `keepap` |

### Time and location

| Field | NVS key | Type | Default | Range / rules | Portal field |
| --- | --- | --- | --- | --- | --- |
| `timezone` | `tz` | String | `TIMEZONE_DEFAULT` | ≤80, POSIX TZ | `tz` |
| `timezoneLabel` | `tzlabel` | String | `TIMEZONE_LABEL_DEFAULT` | ≤24 | `tzlabel` |
| `locator` | `locator` | String | `MAIDENHEAD_LOCATOR` | 4 or 6 chars `AA00` / `AA00aa`, upper-cased | `locator` |
| `clock12Hour` | `clock12` | bool | false | – | `clock12` |
| `swapUtcLocal` | `swaputc` | bool | false | – | `swaputc` |

At load, a stored TZ equal to `CET-1CEST-2,M3.5.0/2,M10.5.0/3` is rewritten to `TIMEZONE_DEFAULT` (migration of the former Central Europe preset). Note that on builds where `TIMEZONE_DEFAULT` is not a CET rule, this turns that preset into the build default.

### Data sources

| Field | NVS key | Type | Default | Range | Portal field |
| --- | --- | --- | --- | --- | --- |
| `useJsonPropagationProxy` | `propjson` | bool | false | – | `propmode` = `json` |
| `propagationJsonUrl` | `propurl` | String | `PROPAGATION_JSON_URL` | ≤180 | `propurl` |
| `propagationRefreshMinutes` | `propmins` | u16 | 15 | 1–120 | `propmins` |
| `dxSourceMode` | `dxmode` | u8 | Auto (2) | 0 JSON, 1 Telnet, 2 Auto | `dxmode` |
| `dxSpotsUrl` | `dxurl` | String | `DX_SPOTS_URL` or `https://web.cluster.iz3mez.it/spots.json` | ≤180 | `dxurl` |
| `dxTelnetHost` | `dxhost` | String | `dxspots.com` | ≤64 | `dxhost` |
| `dxTelnetPort` | `dxport` | u16 | 7300 | 1–65535 | `dxport` |
| `dxRefreshMinutes` | `dxmins` | u16 | 5 | 1–120 | `dxmins` |

### OpenWebRX, DMR, APRS

| Field | NVS key | Type | Default | Range | Portal field |
| --- | --- | --- | --- | --- | --- |
| `openWebRxUrl` | `owrxurl` | String | `OPENWEBRX_URL_DEFAULT` | ≤180 | `owrxurl` |
| `dmrHotspotUrl` | `dmrhotspot` | String | `DMR_HOTSPOT_URL_DEFAULT` | ≤180 | `dmrurl` |
| `dmrRefreshSeconds` | `dmrsecs` | u16 | 30 | 15–600 | `dmrsecs` |
| `aprsRadiusKm` | `aprsradius` | u16 | `APRS_RADIUS_KM_DEFAULT` (100) | 10–300 | `aprsradius` |
| `aprsFiApiKey` | `aprsfikey` | String | `APRSFI_API_KEY_DEFAULT` | ≤64; blank keeps saved key | `aprsfikey`, `clearaprsfikey` |

### PSKReporter, POTA, ISS

| Field | NVS key | Type | Default | Range | Portal field |
| --- | --- | --- | --- | --- | --- |
| `pskDirection` | `pskdir` | u8 | 0 who hears me | 0 / 1 | `pskdir` (`heard` / `hearing`) |
| `pskWindowMinutes` | `pskwin` | u16 | 60 | 5–360 | `pskwin` |
| `pskRefreshMinutes` | `pskmins` | u16 | 5 | 5–120 | `pskmins` |
| `pskAppContact` | `pskmail` | String | `""` | ≤64 | `pskmail` |
| `potaMaxDistanceKm` | `potadist` | u16 | 0 (no limit) | 0–20000 | `potadist` |
| `potaRefreshMinutes` | `potamins` | u16 | 5 | 1–120 | `potamins` |
| `potaExcludeRbn` | `potarbn` | bool | false | – | `potarbn` |
| `issEnabled` | `issenabled` | bool | false | – | `issenabled` |
| `n2yoApiKey` | `n2yokey` | String | `N2YO_API_KEY_DEFAULT` | ≤64 | `n2yokey` |

### Page rotation and display

| Field | NVS key | Type | Default | Range | Portal field |
| --- | --- | --- | --- | --- | --- |
| `autoPageChange` | `autopage` | bool | false | – | `autopage` |
| `autoPageSeconds` | `autosecs` | u16 | 15 | 3–600 | `autosecs` |
| `autoPageMask` | `autopages` | u16 | `0x3FFF` (all 14) | bit *n* = page *n+1* | `pg0` … `pg13` |
| `brightnessPercent` | `bright` | u8 | 100 | 5–100 | `bright` |
| `nightDimEnabled` | `nightdim` | bool | false | – | `nightdim` |
| `nightBrightnessPercent` | `nightpct` | u8 | 20 | 5–100 | `nightpct` |
| `nightFadeMinutes` | `nightfade` | u16 | 40 | 1–240 | `nightfade` |
| `swapRedBlueChannels` | `swaprb` | bool | `SWAP_RED_BLUE_DEFAULT` | – | `swaprb` |
| `rotate90` | `rot90` | bool | `ROTATE90_DEFAULT` | – | `rot90` |
| `flip180` | `flip180` | bool | `FLIP180_DEFAULT` | – | `flip180` |
| `mirror` | `mirror` | bool | `MIRROR_DEFAULT` | – | `mirror` |
| `invertColours` | `invert` | bool | `INVERT_COLOURS_DEFAULT` | – | `invert` |
| `swapTouchNav` | `touchswap` | bool | `SWAP_TOUCH_NAV_DEFAULT` | – | `touchswap` |

`autopages` was stored as an 8-bit value (`putUChar`) before the page count grew past 8; it is now read with `getUShort`. On the first boot after upgrading, the old value is not found and the default (all pages) is used. A mask saved while the dashboard had 13 pages does not include the new APRS Map page (bit 13); tick it under *Automatic Page Change* to add it to the rotation.

## Adding a new setting

1. Add the field to `AppSettings` in `settings.h`.
2. Load it in `settingsBegin()` with a short (≤15 chars) unique NVS key and a default.
3. Clamp/validate it in `normalizeSettings()`.
4. Write it in `saveSettings()`.
5. Add the form field in `pageHtml()` and read it in `handleSave()` (`setup_portal.cpp`).
6. If the module caches anything derived from it, call its `request…Refresh()` from `handleSave()`.

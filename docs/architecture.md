# Architecture

## Overview

The firmware is a single-threaded Arduino sketch for ESP32 boards with a TFT panel and an XPT2046 resistive touch controller (the "Cheap Yellow Display" family). It does not use LVGL, an SD card or a filesystem. All pages are drawn directly with TFT_eSPI, all settings live in ESP32 Preferences (NVS), and the world maps are compiled into flash as RGB565 arrays.

```
            ┌───────────────┐
            │   main.cpp    │ setup() / loop()
            └──────┬────────┘
     ┌─────────────┼────────────────────────────────┐
     ▼             ▼                ▼               ▼
 settings    setup_portal     connectivity     reset_button
 (NVS)       (AP + web UI)    (Wi-Fi, NTP)     (BOOT = factory reset)
     │             │                │
     └──────┬──────┴────────────────┘
            ▼
    dashboard_display  ── owns the TFT, touch, paging, backlight
            │ polls every loop
   ┌────────┼─────────┬──────────┬─────────┬──────────┬──────────┬──────────┐
   ▼        ▼         ▼          ▼         ▼          ▼          ▼          ▼
propagation greyline dx_spots pota_spots psk_reporter iss_tracker dmr_panel aprs_nearby
 (HamQSL)  (local)  (JSON+   (api.pota) (PSKReporter) (Celestrak (OpenWebRX (APRS-IS
                     Telnet)                           + N2YO)    + hotspot)  + APRS.fi)
```

## Boot sequence (`main.cpp`)

1. `Serial.begin(115200)`.
2. `settingsBegin()` – opens the Preferences namespace and loads `AppSettings` (falling back to compile-time defaults from `app_config.h`).
3. `displayBegin()` – initialises TFT_eSPI, allocates sprites, calls every data module's `*Begin()`, starts the touch SPI bus and applies orientation/colour/backlight settings.
4. If any Wi-Fi network is saved, the "searching for Wi-Fi" screen is shown (`displayShowWifiSearching`).
5. `resetButtonBegin()` – GPIO0 input with pull-up.
6. `setupPortalBegin()` – starts the `CYD-HamClock-Setup` soft-AP, captive DNS and the web server (port 80).
7. `connectivityBegin()` – `WIFI_AP_STA` mode, builds the WiFiMulti list, starts the connection **in a FreeRTOS task** so the spinner can animate, and calls `configTzTime()` with the saved POSIX TZ and three NTP servers.
8. The first dashboard frame is drawn unless the Wi-Fi attempt is still running.

## Main loop

`loop()` runs roughly every 10 ms:

```
setupPortalLoop()      DNS + HTTP handling, hotspot auto-off, deferred reconnect/reboot, mDNS
connectivityLoop()     Wi-Fi reconnect every 10 s while disconnected
resetButtonLoop()      BOOT held? show countdown, factory reset after 5 s
[startup] while the first Wi-Fi attempt runs → animate spinner and return
displayUpdate(getClockSnapshot())
delay(10)
```

`displayUpdate()` is where all data work happens:

1. `handleTouch()`.
2. Calls every module's `refresh…IfNeeded()` / `service…()` function and ORs their "data changed" results.
3. `serviceNightDimming()`, `serviceAutoPageChange()`, `serviceOpenWebRxChat()`.
4. Redraws the current page when the page is dirty, data changed, or 250 ms have passed; otherwise it advances the DX/POTA scroll animation.

## Polling model

Every data module follows the same contract:

| Function | Purpose |
| --- | --- |
| `xxxBegin()` | Reset module state to "Waiting". Called once from `displayBegin()`. |
| `refreshXxxIfNeeded(bool wifiConnected)` | Called every loop. Returns immediately unless the refresh interval has elapsed or a refresh was requested; otherwise fetches, parses and updates the module's data struct. Returns `true` when something visible changed. |
| `requestXxxRefresh()` | Sets a flag so the next call fetches (subject to any hard rate floor). Called by centre-taps and by saving settings. |
| `getXxxData()` | Returns a `const&` to the module's data struct, read by the renderer. |

There is no RTOS concurrency in the data path: fetches run synchronously inside `loop()`. HTTP clients use bounded connect/read timeouts (3.5–8 s) so a dead server stalls the UI for a bounded time. The only background tasks are the WiFiMulti connect task (`connectivity.cpp`) and the Telnet DX connect task (`dx_spots.cpp`).

Status strings (e.g. `"OK"`, `"HTTP 503"`, `"No Wi-Fi"`) are stored in each data struct and shown on the page, so failures are visible without a serial console. Most modules keep the last good data when a refresh fails and only update the status line.

### Refresh intervals

| Data | Default | Range / floor | Configurable |
| --- | --- | --- | --- |
| HamQSL propagation | 15 min | 1–120 min | yes |
| DX JSON | 5 min | 1–120 min | yes |
| DX Telnet | persistent; reconnect ≥ 30 s; stale after 10 min silence | – | host/port only |
| PSKReporter | 5 min | 5–120 min (hard 5-min floor) | yes |
| POTA | 5 min | 1–120 min (hard 1-min floor) | yes |
| ISS position/track | 20 s (local SGP4) | fixed | no |
| ISS TLE | 24 h | fixed | no |
| ISS passes (N2YO) | 1 h | fixed | no |
| DMR hotspot + OpenWebRX status | 30 s | 15–600 s | yes |
| APRS-IS | persistent once an APRS page has been opened; reconnect ≥ 30 s | – | radius only |
| APRS map redraw | on new positions, at most every 2 s; stations expire after 60 min | fixed | no |
| APRS.fi weather | ≤ once per 15 min, only while the weather page is shown | fixed | key only |
| Greyline | 1 min | fixed | no |

## Rendering model

`dashboard_display.cpp` keeps one `String g_lastXxx` per on-screen field. The helpers `drawCenteredField`, `drawLeftField`, `drawSplitField`, etc. compare the new text with the cached one and only clear and redraw that rectangle when it changed. `clearPageState()` empties all caches when the page changes, and `g_pageDirty` forces a full background paint. This keeps SPI traffic low and avoids flicker.

The map pages (Greyline, PSKReporter, ISS) compose the map in horizontal bands (`MAP_BAND_ROWS`) inside a sprite and push each band, because the 4.0" map (460×230×2 = 211 600 bytes) does not fit in the largest free heap block. A *map signature* string (QTH, sun position, report count…) decides whether the map must be recomposed.

DX and POTA share one row model and one scroll sprite: new spots slide in from the top with a sprite-based animation (`startDxScroll` / `stepDxScroll`) that is advanced between full renders.

Layouts are derived from `DISPLAY_W` / `DISPLAY_H`, which come from the force-included `User_Setup*.h`, with `#if DISPLAY_W >= 480` branches for the 4.0" panel. The five newer pages (OpenWebRX, DMR, World Clock, APRS Weather, Nearby APRS) use fixed 320×240 coordinates and do not yet scale to the 480×320 panel.

## Build targets

| PlatformIO env | Board | Panel | Setup header |
| --- | --- | --- | --- |
| `esp32-2432s028r` | ESP32-2432S028R 2.8" | ILI9341 320×240 | `include/User_Setup.h` |
| `esp32-2432s028r-st7789` | ESP32-2432S028R 2.8" | ST7789 320×240 | `include/User_Setup_ST7789.h` |
| `esp32-4in-st7796` | CYD 4.0" | ST7796S 480×320 | `include/User_Setup_ST7796.h` |

All environments: `espressif32` / Arduino, `board = esp32dev`, partition table `min_spiffs.csv` (≈1.9 MB app slot, needed because the embedded maps are large). Libraries: TFT_eSPI 2.5.43, ArduinoJson 6.21, Sgp4 1.0.3, arduinoWebSockets 2.4.1.

Each environment force-includes its setup header (`-include include/User_Setup*.h` with `-D USER_SETUP_LOADED=1`), so `DISPLAY_W`, `DISPLAY_H`, pin numbers and panel driver macros are visible in every translation unit. The ST7796 build also selects `greyline_map_460x230.h` instead of `greyline_map.h` (300×150).

Approximate flash use (arduino-esp32 2.0.17): 71 % of the app partition on the 2.8" builds, 77 % on the 4.0" build.

## Memory and networking notes

- All TLS clients go through `configureSecureClient()`, which calls `setInsecure()` – certificates are **not** validated. Each TLS session costs ~32 KB of heap (two 16 KB mbedTLS record buffers).
- Large feeds are streamed: POTA reads one JSON object at a time, PSKReporter one XML element at a time, OpenWebRX status uses an ArduinoJson filter. DX JSON is buffered (up to 24 KB document) after freeing the TLS buffers first. The WPSD fallback in `dmr_panel.cpp` reads the whole HTML page into a `String`.
- `/status` reports `free_heap` and `max_alloc` (largest free block) to help diagnose fragmentation.
- The web UI has no authentication; it is meant for a trusted LAN.

## Source tree

```
include/
  app_config.h             Compile-time defaults (#ifndef fallbacks); includes app_config.local.h if present
  app_config.example.h     Template for app_config.local.h
  app_config.local.h       Per-device secrets and defaults (git-ignored)
  User_Setup.h             TFT_eSPI setup, 2.8" ILI9341
  User_Setup_ST7789.h      TFT_eSPI setup, 2.8" ST7789
  User_Setup_ST7796.h      TFT_eSPI setup, 4.0" ST7796S (+ touch calibration)
  greyline_map.h           300x150 RGB565 world map
  greyline_map_460x230.h   460x230 RGB565 world map (4.0")
src/
  main.cpp                 setup()/loop()
  connectivity.*           Wi-Fi, NTP, timezone, TLS client setup
  settings.*               AppSettings + Preferences
  setup_portal.*           Soft-AP, captive portal, web settings, /status
  reset_button.*           BOOT-button factory reset
  dashboard_display.*      All rendering, touch, paging, backlight
  propagation.*            HamQSL / JSON proxy
  greyline.*               Solar geometry and Maidenhead maths
  dx_spots.*               DX JSON + Telnet cluster
  pota_spots.*             POTA spots
  psk_reporter.*           PSKReporter reports
  iss_tracker.*            ISS SGP4 + N2YO passes
  dmr_panel.*              OpenWebRX + DMR hotspot
  aprs_nearby.*            APRS-IS nearby stations/weather
tools/
  apply_dmr_patch.py       Legacy overlay script (out of date – see modules/dmr-openwebrx.md)
```

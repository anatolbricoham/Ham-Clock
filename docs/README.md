# CYD Ham Dashboard – BricoHams edition – Documentation

This folder documents the firmware in `src/` and `include/` and explains how to configure a device.

| Document | Audience | What it covers |
| --- | --- | --- |
| [Configuration manual](configuration-manual.md) | Operators | Board selection, building/flashing, first boot, every web-settings field, per-page setup, troubleshooting, factory reset |
| [Architecture](architecture.md) | Developers | Boot sequence, main loop, module map, polling model, rendering model, memory/network design, build targets |
| [Settings reference](modules/settings.md) | Both | Every setting: NVS key, type, default, valid range, where it is edited |

## Module reference

| Module | Files | Responsibility |
| --- | --- | --- |
| [Entry point, connectivity, reset button](modules/core.md) | `main.cpp`, `connectivity.*`, `reset_button.*` | Startup order, main loop, Wi-Fi (WiFiMulti), NTP, timezone, BOOT-button factory reset |
| [Settings](modules/settings.md) | `settings.*`, `include/app_config*.h` | Preferences (NVS) storage, compile-time defaults, validation |
| [Setup portal](modules/setup-portal.md) | `setup_portal.*` | Captive-portal hotspot, LAN web settings page, `/status` JSON, mDNS |
| [Dashboard display](modules/dashboard-display.md) | `dashboard_display.*`, `include/User_Setup*.h`, `include/greyline_map*.h` | TFT_eSPI rendering of all 14 pages, touch input, paging, auto page change, backlight/night dimming, panel orientation |
| [Propagation](modules/propagation.md) | `propagation.*` | HamQSL XML (or JSON proxy) solar/band data for the HF and VHF pages |
| [Greyline](modules/greyline.md) | `greyline.*` | Maidenhead → lat/lon, sunrise/sunset, subsolar point, night fraction, great-circle distance |
| [DX spots](modules/dx-spots.md) | `dx_spots.*` | DX spots from a JSON feed and/or a persistent Telnet DX Cluster connection |
| [POTA spots](modules/pota-spots.md) | `pota_spots.*` | Parks on the Air activator spots, streaming JSON parse, distance/RBN filter |
| [PSKReporter](modules/psk-reporter.md) | `psk_reporter.*` | Reception reports for the station callsign, streaming XML parse, 5-minute rate floor |
| [ISS tracker](modules/iss-tracker.md) | `iss_tracker.*` | Celestrak TLE + on-device SGP4 position/ground track, N2YO pass predictions |
| [DMR & OpenWebRX](modules/dmr-openwebrx.md) | `dmr_panel.*` | OpenWebRX status + chat (WebSocket), DMR hotspot "last heard" (Pi-Star JSON or WPSD HTML) |
| [APRS nearby](modules/aprs-nearby.md) | `aprs_nearby.*` | Receive-only APRS-IS radius feed, nearest weather station, nearby stations, live APRS map data, optional APRS.fi enrichment |

## Page map

| # | Page | Data module | Centre-tap action |
| ---: | --- | --- | --- |
| 1 | Clock | connectivity | – |
| 2 | HF Propagation | propagation | Refresh propagation |
| 3 | VHF Conditions | propagation | Refresh propagation |
| 4 | Greyline | greyline | – |
| 5 | PSKReporter | psk_reporter | Queue refresh (honours 5-min floor) |
| 6 | ISS Tracker *(hidden until enabled + N2YO key)* | iss_tracker | Refresh TLE/position/passes |
| 7 | DX Spots | dx_spots | Refresh DX |
| 8 | POTA Spots | pota_spots | Refresh POTA |
| 9 | OpenWebRX | dmr_panel | – |
| 10 | DMR | dmr_panel | – |
| 11 | World Clock | (local) | – |
| 12 | APRS Weather | aprs_nearby | – |
| 13 | Nearby APRS | aprs_nearby | – |
| 14 | APRS Map | aprs_nearby | Cycle zoom (full / ½ / ¼ radius) |

Tapping the left or right half of the screen (outside the centre third on pages that have a centre action) moves to the previous/next page. See [dashboard display](modules/dashboard-display.md#touch-input).

## Credits

BricoHams edition, maintained by the **BricoHams** group, with thanks to **EA5JEF, Diego**. Based on the original [CYD Ham Dashboard by HenrysCat](https://github.com/HenrysCat/esp32-cyd-ham-dashboard) (GPL-3.0). Full acknowledgements, libraries and data services: [CREDITS.md](../CREDITS.md).

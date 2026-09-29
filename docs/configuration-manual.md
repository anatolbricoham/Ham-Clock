# Configuration manual

This manual takes you from a bare board to a fully configured dashboard. Settings can be made in two places:

- **At build time**, in `include/app_config.local.h` – defaults and secrets baked into the firmware.
- **At run time**, in the device's web settings page – saved in the ESP32's flash and kept across reboots. A saved value always overrides the build-time default.

For the full list of settings with storage keys and ranges, see the [settings reference](modules/settings.md).

---

## 1. Choose your board

| Your board | Build environment |
| --- | --- |
| ESP32-2432S028R 2.8", ILI9341 panel (most common CYD) | `esp32-2432s028r` |
| ESP32-2432S028R 2.8", ST7789 panel | `esp32-2432s028r-st7789` |
| CYD 4.0" 480×320, ST7796S panel | `esp32-4in-st7796` |

If you don't know which 2.8" panel you have, try `esp32-2432s028r` first. If the screen stays white or shows noise, try the ST7789 build.

## 2. Build-time configuration (optional)

Copy the template and edit it. The file is ignored by Git, so your passwords and keys stay private.

```sh
cp include/app_config.example.h include/app_config.local.h      # Windows: copy include\app_config.example.h include\app_config.local.h
```

```cpp
#pragma once
#define WIFI_SSID "MyHomeWiFi"
#define WIFI_PASSWORD "secret"
#define MAIDENHEAD_LOCATOR "IM98ib"
#define APP_SETTINGS_NAMESPACE "cyd-hamclock"   // see note below
#define TIMEZONE_DEFAULT "CET-1CEST-2,M3.5.0/2,M10.5.0/3"
#define TIMEZONE_LABEL_DEFAULT "Madrid"
#define CALLSIGN_DEFAULT "EA5XXX"
#define OPENWEBRX_URL_DEFAULT "http://sdr.example.org:8073"
#define DMR_HOTSPOT_URL_DEFAULT "http://pi-star.local"
#define N2YO_API_KEY_DEFAULT ""
#define APRS_RADIUS_KM_DEFAULT 100
#define APRSFI_API_KEY_DEFAULT ""
#define PROPAGATION_JSON_URL ""
#define DX_SPOTS_URL ""

// Display defaults for this board (applied on first boot and after a factory reset)
// 2.8" ILI9341 usually needs:
// #define ROTATE90_DEFAULT true
// #define MIRROR_DEFAULT true
// #define INVERT_COLOURS_DEFAULT true
// 4.0" ST7796 usually needs:
// #define ROTATE90_DEFAULT true
// #define FLIP180_DEFAULT true
// #define SWAP_RED_BLUE_DEFAULT true
```

Everything here is optional – you can leave the file out and do all configuration from the web page.

> **Upgrading from an older firmware?** Older releases stored settings under the namespace `hamclock`. If you want to keep the Wi-Fi networks and settings already saved on the device, set `#define APP_SETTINGS_NAMESPACE "hamclock"`. Any other value starts with empty settings.

## 3. Build and flash

With VS Code + PlatformIO, or from a terminal:

```sh
pio run -e esp32-2432s028r -t upload     # use your environment from step 1
pio device monitor                        # optional: serial log at 115200 baud
```

Always pass `-e`; without it PlatformIO builds and uploads all three environments in turn.

To flash a release binary without building:

```sh
python -m pip install esptool
esptool.py --chip esp32 --port COM5 --baud 460800 write_flash -z 0x10000 firmware.bin
```

## 4. First boot and Wi-Fi

1. On power-up the board starts the hotspot **`CYD-HamClock-Setup`**, password **`hamclock`**.
2. Join it from a phone or PC. The captive portal usually opens by itself; otherwise open `http://192.168.4.1`.
3. In **Station**, enter your callsign and at least one Wi-Fi network, then **Save settings**.
4. The board joins your network. About 8 seconds after the connection is confirmed the hotspot switches off.
5. The Clock page shows the device's IP address. From now on, open `http://<that IP>/` or `http://cyd-ham.local/` from your LAN.

If the board can't join any saved network, the hotspot stays on so you can fix the settings.

## 5. Web settings page, section by section

The top card shows live status: Wi-Fi, NTP, IP, mDNS name, uptime, free memory, current page, propagation/DX status and hotspot state.

### Station

| Field | What to enter |
| --- | --- |
| Callsign | Your callsign. Used for DX Telnet login (`NOCALL` if blank), PSKReporter queries and APRS-IS login. |
| Wi-Fi SSID 1–5 / Password | Up to five networks; the strongest known network in range is joined. Passwords are never shown – leave the field blank to keep the saved one, tick *Clear this password* for an open network, clear the SSID to delete the entry. |
| Keep the setup hotspot switched on | Leave the `CYD-HamClock-Setup` hotspot running permanently (e.g. to reach the page without your router). |

### Time and Location

| Field | What to enter |
| --- | --- |
| Timezone preset | Picks a rule and a label for you (UTC, UK, Ireland, Spain/Central Europe, Eastern Europe, US zones, Canada Atlantic, Australia, New Zealand, Japan, China, India, Brazil East, South Africa). |
| Timezone label | Text shown next to local time, e.g. `Madrid`. |
| POSIX timezone rule | E.g. `CET-1CEST-2,M3.5.0/2,M10.5.0/3` (Spain peninsula), `WET0WEST,M3.5.0/1,M10.5.0/2` (Canary Islands), `UTC0`. |
| Maidenhead locator | 4 or 6 characters, e.g. `IM98` or `IM98ib`. Used for greyline, distances, PSKReporter, POTA filter, ISS passes, APRS radius and night dimming. |
| 12-hour clock | Local time with AM/PM (UTC stays 24 h). |
| Swap UTC and local time | Local time becomes the big readout; the footer shows local time instead of UTC. |

### Data Sources

| Field | Default | Notes |
| --- | --- | --- |
| Propagation source mode | Direct HamQSL XML | Or *JSON proxy URL* – see [propagation](modules/propagation.md#json-proxy-format) for the format. |
| Propagation JSON URL | empty | Only used in JSON mode. |
| DX source mode | Auto (JSON then Telnet) | *JSON only* polls the feed; *Telnet only* keeps a live cluster connection. |
| DX JSON URL | `https://web.cluster.iz3mez.it/spots.json` | Any feed with the same JSON array format. |
| DX Telnet host / port | `dxspots.com` / `7300` | Any DX Spider / AR-Cluster node that accepts a callsign login. |
| Propagation refresh minutes | 15 | 1–120. |
| DX refresh minutes | 5 | 1–120 (JSON polling). |

### OpenWebRX

| Field | Notes |
| --- | --- |
| OpenWebRX base URL | E.g. `http://192.168.1.50:8073`. Do **not** add `/status.json`. Leave empty to disable. The page shows receiver status and the live chat while it is on screen. |

### DMR Last Heard (Live)

| Field | Default | Notes |
| --- | --- | --- |
| DMR hotspot base URL | empty | E.g. `http://pi-star.local` or `http://192.168.1.60`. Do **not** add `/api/last_heard.php`. Works with Pi-Star (JSON API) and WPSD (HTML table fallback). |
| Refresh seconds | 30 | 15–600. |

Leave unused URLs empty: an unreachable server makes the display pause while it waits for the timeout.

### APRS Nearby

| Field | Default | Notes |
| --- | --- | --- |
| APRS nearby radius (km) | 100 | 10–300, centred on your locator. |
| APRS.fi API key | empty | Optional, personal key from aprs.fi → *Account*. Enriches the nearest weather station while the weather page is open (max. once per 15 min). Blank keeps the saved key; tick *Clear* to remove it. |

Needs a callsign and locator. The connection is receive-only; nothing is transmitted.

The radius also sets the scale of the **APRS Map** page: a live range-ring map centred on your QTH (north up) showing every station heard inside the radius during the last hour – cyan squares for weather stations, yellow triangles for digipeaters/nodes, green dots for other stations, grey when not heard for 30 minutes – with a nearest-first list of callsign, distance and direction beside it. Tap the centre of the screen to zoom in to ½ and ¼ of the radius. If you already use automatic page change, tick *APRS Map* in the page list to include it.

### PSKReporter

| Field | Default | Notes |
| --- | --- | --- |
| Direction | Who is hearing me | Or *Who I am hearing*. |
| Report window minutes | 60 | 5–360. |
| Refresh minutes | 5 | 5–120; five minutes is the service's minimum. |
| Contact email | empty | Optional; lets PSKReporter contact you instead of blocking the device. |

### POTA

| Field | Default | Notes |
| --- | --- | --- |
| Max distance km | 0 | 0 = no limit. Spots without coordinates are always shown. |
| Refresh minutes | 5 | 1–120. |
| Hide RBN spots | off | Show only human-posted spots. |

### ISS Tracker

Tick *Show ISS tracker page* and paste a free key from `n2yo.com/api`. The page is hidden until both are set.

### Automatic Page Change

| Field | Default | Notes |
| --- | --- | --- |
| Change pages automatically | off | |
| Seconds on each page | 15 | 3–600. |
| Pages included | all | Unticked pages are skipped by the rotation but still reachable by tapping. Any tap restarts the countdown. |

### Display

| Field | Default | Notes |
| --- | --- | --- |
| Backlight brightness % | 100 | 5–100 (daytime level). |
| Dim the backlight at night | off | Fades to the night level around sunset/sunrise at your locator. |
| Night brightness % | 20 | 5–100. |
| Fade minutes | 40 | 1–240, centred on each sunrise/sunset. |
| Swap red/blue | – | See §6. |
| Rotate 90° / Flip 180° / Mirror / Invert colours | – | See §6. |
| Swap left/right page navigation | off | If taps move the wrong way. |

Changes apply immediately when you press **Save settings**; no restart is needed. **Restart device** reboots the board.

## 6. Display orientation and colours

Use the Display section until the picture is right. Each change applies on save.

| Symptom | Tick |
| --- | --- |
| Portrait, cropped picture | Rotate display 90° |
| Upside down | Flip display 180° |
| Text reads backwards | Mirror display |
| Colours look like a photo negative | Invert display colours |
| Red shows as blue, yellow as cyan | Swap red/blue display channels |
| Tapping left/right goes the wrong way | Swap left/right page navigation |

Typical combinations: 2.8" ILI9341 → *Rotate 90° + Mirror + Invert*; 4.0" ST7796 → *Rotate 90° + Flip 180° + Swap red/blue*. To make them survive a factory reset, set the matching `*_DEFAULT` macros in `app_config.local.h` (§2).

If you can't read the screen to find the IP address, join the `CYD-HamClock-Setup` hotspot (it is on until Wi-Fi works) and open `http://192.168.4.1`.

## 7. Using the dashboard

- **Tap the left half** of the screen → previous page. **Right half** → next page (swap with the setting above).
- **Tap the centre third** to refresh: HF/VHF propagation, DX, POTA, PSKReporter (queued until 5 min have passed), ISS. On the **APRS Map** page a centre tap cycles the zoom (full radius → ½ → ¼).
- The footer shows Wi-Fi, NTP, page number `n/14` and UTC (or local) time.

| # | Page | Needs |
| ---: | --- | --- |
| 1 | Clock | – |
| 2 | HF Propagation | Internet |
| 3 | VHF Conditions | Internet |
| 4 | Greyline | Locator, NTP |
| 5 | PSKReporter | Callsign, locator |
| 6 | ISS Tracker | Enabled + N2YO key |
| 7 | DX Spots | Internet |
| 8 | POTA Spots | Internet |
| 9 | OpenWebRX | OpenWebRX URL |
| 10 | DMR | Hotspot URL |
| 11 | World Clock | NTP |
| 12 | APRS Weather | Callsign, locator |
| 13 | Nearby APRS | Callsign, locator |
| 14 | APRS Map | Callsign, locator |

## 8. Status endpoint

`http://<device>/status` returns JSON with Wi-Fi, IP, uptime, free heap, largest free block, NTP, current page, backlight level, sun state and propagation/DX status – handy for monitoring. See [setup portal](modules/setup-portal.md#status-json).

## 9. Factory reset

Hold the **BOOT** button (back of the board, next to USB) for **5 seconds** while the dashboard is running. A countdown is shown; release early to cancel. All saved settings are erased and the board restarts with the build-time defaults and the setup hotspot.

## 10. Troubleshooting

| Problem | What to check |
| --- | --- |
| Screen white / noise | Wrong build environment (§1). On a 4.0" board keep `USE_HSPI_PORT` in its setup header. |
| Picture rotated, mirrored, wrong colours | §6. |
| Touch doesn't respond or hits the wrong place | Touch pins/calibration in the board's `User_Setup*.h` (`TOUCH_RAW_MIN/MAX`, `TOUCH_SWAP_XY`, `TOUCH_INVERT_X/Y`). |
| Footer shows `NTP --` | Wi-Fi connected? UDP 123 allowed by your router? |
| Propagation `Fetch failed` | Internet access; try again with a centre-tap. |
| DX `Last good` | The source failed; the last good list is shown. Check URL / Telnet host. |
| PSKReporter `Rate limited` | Increase the refresh interval; add a contact e-mail. |
| PSKReporter `No callsign` | Set the callsign. |
| APRS `Set callsign and locator` | Both are required. |
| APRS `APRS-IS unavailable` | Outbound TCP 14580 blocked? It retries every 30 s. |
| DMR `Connect failed` / `HTTP 404` | Base URL only, no path; hotspot reachable from the board's network? |
| Display freezes for several seconds periodically | An OpenWebRX or DMR URL points to a server that is down – fix or clear it. |
| All settings gone after an update | NVS namespace changed – see the note in §2. |
| Settings page unreachable | Use the IP on the Clock page, or join the hotspot; if it is off and Wi-Fi fails, the hotspot returns. As a last resort, factory reset (§9). |

## 11. Security notes

- The settings page has **no password**. Anyone on your LAN – or connected to the hotspot, whose password is public – can change settings. Let the hotspot switch off after setup and keep the device on a trusted network.
- HTTPS certificates are not verified by the firmware.
- Keep API keys and Wi-Fi passwords in `app_config.local.h` (git-ignored) or enter them in the web page; never commit them.

---

*CYD Ham Dashboard – BricoHams edition. Based on the original project by [HenrysCat](https://github.com/HenrysCat/esp32-cyd-ham-dashboard). Thanks to EA5JEF, Diego. See [CREDITS.md](../CREDITS.md).*

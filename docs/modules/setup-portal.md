# Setup portal and web settings

Files: `src/setup_portal.h`, `src/setup_portal.cpp`

## Public API

```cpp
void setupPortalBegin();   // start soft-AP, DNS and HTTP server
void setupPortalLoop();    // call every loop()
```

## Hotspot

| Constant | Value |
| --- | --- |
| `kApSsid` | `CYD-HamClock-Setup` |
| `kApPassword` | `hamclock` |
| `kDnsPort` | 53 (captive DNS answers every name with the AP IP) |
| `kApAutoOffConfirmMs` | 8000 ms |

- The hotspot always starts at boot (`startHotspot()`, `WIFI_AP_STA`). The portal is at `http://192.168.4.1`.
- When the STA has an IP for 8 s continuously, `stopHotspot()` turns the AP off and switches to `WIFI_STA` – unless *Keep the setup hotspot switched on* (`keepHotspotOn`) is set, in which case the AP is (re)started and kept.
- The hotspot password is fixed in the source and printed on the status card.

## HTTP routes

| Method | Path | Handler | Response |
| --- | --- | --- | --- |
| GET | `/` | `handleRoot` | Settings page (`?saved=1` / `?rebooting=1` show a banner) |
| POST | `/save` | `handleSave` | Saves, applies, 303 → `/?saved=1` |
| GET | `/status` | `handleStatusJson` | JSON status (below) |
| POST | `/reboot` | `handleReboot` | Restart after 1.5 s, 303 → `/?rebooting=1` |
| GET | `/reboot` | `handleRebootGet` | 303 → `/` (prevents accidental GET reboot) |
| GET | `/generate_204`, `/gen_204` | captive redirect | Android captive-portal probe |
| GET | `/hotspot-detect.html` | `handleRoot` | Apple probe |
| GET | `/ncsi.txt` | inline | Windows probe |
| * | anything else | `handleCaptiveRedirect` | 302 to the AP IP while the hotspot is on, otherwise the settings page |

mDNS: once the STA is connected, `cyd-ham.local` is registered with an `_http._tcp` service.

### `/status` JSON

```json
{
  "project": "CYD HamClock",
  "wifi": true,
  "ip": "192.168.1.42",
  "uptime": "3:12:05",
  "free_heap": 123456,
  "max_alloc": 65524,
  "ntp": true,
  "page": 7,
  "backlight": 100,
  "sun": "Daylight",
  "propagation_status": "OK",
  "dx_status": "Reading",
  "dx_source": "Telnet"
}
```

`max_alloc` is the largest contiguous free block – useful when sprites or JSON documents fail with "NoMemory" even though `free_heap` looks healthy.

## Settings page (`pageHtml`)

Built as one `String` (reserved 13 KB) with inline CSS and a small script for timezone presets. Sections, in order:

1. **Status card** – Wi-Fi, NTP, LAN IP, mDNS, uptime, free heap, current page, propagation/DX status, hotspot state and credentials.
2. **Station** – callsign, five Wi-Fi SSID/password slots, keep-hotspot checkbox.
3. **Time and Location** – timezone preset (JavaScript fills the two fields below), label, POSIX rule, locator (HTML `pattern` validation), 12-hour clock, swap UTC/local.
4. **Data Sources** – propagation mode + JSON URL, DX mode + JSON URL + Telnet host/port, propagation and DX refresh minutes.
5. **OpenWebRX** – base URL.
6. **DMR Last Heard (Live)** – hotspot base URL, refresh seconds.
7. **APRS Nearby** – radius, APRS.fi key (write-only) and clear checkbox.
8. **PSKReporter** – direction, window, refresh, contact e-mail.
9. **POTA** – max distance, refresh, hide RBN.
10. **ISS Tracker** – enable, N2YO key.
11. **Automatic Page Change** – enable, seconds, one checkbox per page (`dashboardPageName()`).
12. **Display** – brightness, night dimming (level, fade), swap R/B, rotate 90°, flip 180°, mirror, invert, swap touch navigation.
13. Save button, Restart button.

Secrets handling: Wi-Fi passwords and the APRS.fi key are never sent back to the browser; an empty field keeps the saved value and a separate checkbox clears it. The N2YO key **is** shown in clear text.

All user text is passed through `htmlEscape()`; `/status` uses `jsonEscape()`.

## Save flow (`handleSave`)

1. Copy current settings; overwrite each field from the form (`limitedArg()` trims and truncates; numbers are `constrain`ed).
2. `saveSettings()` → normalise + write NVS.
3. Apply immediately: `applyTimezoneSettings()`, `applyDisplaySettings()` (orientation, colours, backlight, restarts the DMR panel timer), and `request…Refresh()` for propagation, DX, greyline, PSKReporter, POTA and DMR.
4. If any SSID or password changed: `reloadWifiNetworks()` and a reconnect 1.5 s later (after the HTTP response has been sent).
5. Redirect to `/?saved=1`.

## Security

There is no authentication and no CSRF protection. Anyone who can reach the device on the LAN – or join the hotspot, whose password is public in the source – can change settings or restart it. Keep the device on a trusted network and let the hotspot switch off after setup.

# Entry point, connectivity and reset button

Files: `src/main.cpp`, `src/connectivity.h`, `src/connectivity.cpp`, `src/reset_button.h`, `src/reset_button.cpp`

## main.cpp

Owns the Arduino `setup()` / `loop()` and the startup "searching for Wi-Fi" screen. See [architecture](../architecture.md#boot-sequence-maincpp) for the full order.

| Constant | Value | Meaning |
| --- | --- | --- |
| `kLoopDelayMs` | 10 ms | Delay at the end of every `loop()` |
| `kWifiSpinnerIntervalMs` | 180 ms | Frame interval of the startup spinner |

Behaviour worth knowing:

- The spinner screen is shown only when at least one Wi-Fi network is saved. While `wifiConnectionInProgress()` is true, `loop()` skips `displayUpdate()` entirely.
- When the first attempt finishes (success or failure) `requestDisplayRedraw()` forces a full paint of the dashboard.
- `displayUpdate()` is skipped while the BOOT button is held so the reset countdown is not overdrawn.

## connectivity

### Public API

```cpp
struct ClockSnapshot {
  bool wifiConnected;      // WiFi.status() == WL_CONNECTED
  bool timeValid;          // epoch >= 2024-01-01 (NTP has synced)
  time_t epoch;            // time(nullptr)
  uint32_t uptimeSeconds;  // millis() / 1000
};

void connectivityBegin();          // Wi-Fi + NTP start
void connectivityLoop();           // periodic reconnect
bool wifiConnectionInProgress();   // WiFiMulti task running
void reloadWifiNetworks();         // rebuild WiFiMulti AP list after settings change
void reconnectWifi();              // drop STA and start a new connect attempt
void applyTimezoneSettings();      // setenv("TZ") + tzset() from settings
ClockSnapshot getClockSnapshot();
void configureSecureClient(WiFiClientSecure& client);  // setInsecure()
```

### Wi-Fi

- Mode is `WIFI_AP_STA`; `WiFi.persistent(false)` so credentials are never written by the Wi-Fi driver itself (they live in the app's Preferences).
- Up to five networks (`kMaxWifiNetworks`) are added to a `WiFiMulti` instance. `WiFiMulti::run()` scans and joins the strongest known network. Because that call blocks for several seconds it is run in a FreeRTOS task (`wifi-connect`, 4 KB stack); `g_wifiConnectInProgress` tracks it.
- `configureWifiNetworks()` deletes and recreates the `WiFiMulti` object so edited or removed networks take effect without a reboot. If a connect task is running, the reload (and any reconnect) is deferred until it finishes.
- While credentials exist and the STA is not connected, `connectivityLoop()` retries every `kReconnectIntervalMs` (10 s).
- The Wi-Fi *mode* (AP on/off) is owned by `setup_portal`; `reconnectWifi()` only touches the STA side.

### Time

- `configTzTime(tz, "pool.ntp.org", "time.nist.gov", "time.google.com")` is called once at start. SNTP then keeps the clock in sync.
- Time is considered valid once `time(nullptr) >= 1704067200` (2024-01-01).
- The timezone is a POSIX TZ rule (e.g. `CET-1CEST-2,M3.5.0/2,M10.5.0/3`). `applyTimezoneSettings()` re-applies it after settings are saved.
- Note: the World Clock page temporarily changes `TZ` to compute each city and restores it from settings afterwards.

### TLS

`configureSecureClient()` is the single place every HTTPS client is configured. It calls `setInsecure()`, so server certificates are not verified. The 2.0.x core offers no API to shrink mbedTLS buffers, so each TLS connection uses ~32 KB of heap.

## reset_button

GPIO0 (the BOOT button next to the USB connector) is reused at runtime as a factory-reset button.

| Constant | Value |
| --- | --- |
| `kResetButtonPin` | 0 |
| `kHoldMsToReset` | 5000 ms |
| `kFeedbackIntervalMs` | 250 ms (countdown refresh) |

- `resetButtonLoop()` returns `true` while the button is held; the caller pauses dashboard rendering.
- While held, the screen shows "Hold to reset – Ns - release to cancel". Releasing early calls `requestDisplayRedraw()`.
- After 5 s: "Factory reset – Restoring defaults…", `factoryResetSettings()` (clears the whole Preferences namespace, then reloads defaults) and `ESP.restart()`.

# DX spots

Files: `src/dx_spots.h`, `src/dx_spots.cpp`

## Public API

```cpp
constexpr uint8_t kMaxDxSpots = 8;   // 12 when DISPLAY_H >= 320
void dxSpotsBegin();
bool refreshDxSpotsIfNeeded(bool wifi);
void requestDxSpotsRefresh();
const DxSpotsData& getDxSpotsData();
String getDxSpotsUrl();              // saved URL, DX_SPOTS_URL, or the IZ3MEZ default
```

`DxSpot`: `time`, `freq` (MHz string), `call`, `mode`, `spotter`, `comment`, `band`, `country`, `continent`.
`DxSpotsData`: `hasData`, `status`, `updated`, `source` (`JSON`, `Telnet` or `Last good`), `provider` (short name derived from the host, e.g. `IZ3MEZ`, `DXSPOTS`), `spotCount`, `spots[]` (newest first).

## Source modes (`dxSourceMode`)

| Mode | Behaviour |
| --- | --- |
| **Auto** (default) | First fetches the JSON feed so the list is full within seconds of boot. Then opens the Telnet cluster; live spots are pushed on top of the JSON list. While Telnet has not yet delivered a spot, JSON keeps refreshing on the DX interval. Telnet is never torn down to re-poll JSON. Telnet connection errors do not overwrite a working JSON list's status. |
| **JSON** | Polls the JSON URL every `dxRefreshMinutes`; Telnet is closed. |
| **Telnet** | Keeps a persistent cluster connection; a manual refresh drops and reconnects it. |

Changing the mode empties the list so spots from the old source are never shown under the new label.

## JSON feed

- Default URL `https://web.cluster.iz3mez.it/spots.json` (override with *DX JSON URL* or `DX_SPOTS_URL`). HTTP and HTTPS are supported; redirects are followed; timeout 5 s.
- Expected format: a JSON array of objects with `spot_time`, `spot_datetime`, `frequency`, `spotted`, `spotter`, `spotter_comment`, `band`, `spotted_country`, `spotted_continent`.
- `readDxArrayPrefix()` copies only the first objects (each ≤ 4 KB) from the stream, the TLS client is closed, then the buffered prefix is parsed with a 24 KB `DynamicJsonDocument`.
- Failures set `Fetch failed` / `Parse failed` / `DX URL not set` and keep the previous list labelled `Last good`.

## Telnet cluster

| Constant | Value |
| --- | --- |
| Default host / port | `dxspots.com:7300` |
| Connect timeout | 5 s (runs in a FreeRTOS task, polled by `pollDxTelnetConnect`) |
| Reconnect interval | 30 s |
| Login delay | 1.5 s after connect, or immediately on a `login:` / `call:` / `callsign:` / `please enter your call` prompt |
| Stale timeout | 10 min without any byte → reconnect (catches NAT-dropped sockets) |
| Read budget | 512 bytes per loop, lines ≤ 180 chars (longer lines dropped) |

Login sends the station callsign, or `NOCALL` when none is set. Lines starting with `DX de ` are parsed by `parseDxClusterSpotLine()` (spotter, frequency in kHz, call, comment, `HHMMZ` time). Duplicates (same call, frequency and time) are ignored. Status texts: `Connecting`, `Connected`, `Login sent`, `Reading`, `Disconnected`, `Stalled`.

## Mode detection (`deriveMode`)

1. Comment contains one of `FT8 FT4 CW SSB USB LSB RTTY SSTV PSK` as a whole token.
2. Otherwise frequency within tolerance of 7.074/14.074/21.074/28.074 MHz → FT8, or 7.047/14.080/21.080/28.080 → FT4.
3. Otherwise `--`.

## Display

Rendered by `drawDxPage()`; new spots scroll in (see [dashboard display](dashboard-display.md#dxpota-scrolling)). A centre-tap calls `requestDxSpotsRefresh()`. Define `DEBUG_DX_TELNET 1` to log every Telnet line to the serial port.

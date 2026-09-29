# APRS nearby

Files: `src/aprs_nearby.h`, `src/aprs_nearby.cpp`

Feeds the **APRS Weather** (page 12), **Nearby APRS** (page 13) and **APRS Map** (page 14) pages.

## Public API

```cpp
constexpr uint8_t kMaxNearbyAprsStations = 6;
constexpr uint8_t kMaxAprsMapStations = 24;
constexpr uint32_t kAprsMapMaxAgeMs = 60UL * 60UL * 1000UL;
void aprsNearbyBegin();
bool serviceAprsNearby(bool wifi, bool pageActive, bool weatherPageActive);
const NearbyAprsData& getNearbyAprsData();
```

`NearbyAprsData`: `status`, `weatherSource` (`APRS-IS` / `APRS.fi`), `weatherApiStatus`, `stations[6]` (`callsign`, `comment`, `distanceKm`, `node`), `stationCount`, `weather`, `mapStations[24]`, `mapStationCount`, `mapRevision`.
`AprsMapStation`: `callsign`, `latitude`, `longitude`, `distanceKm`, `bearingDeg` (from the QTH, 0 = north), `heardMs` (`millis()` of the last position), `symbolTable`, `symbol`, `node`, `weather`.
`NearbyAprsWeather`: `callsign`, `distanceKm`, `receivedAt`, and `has…` flags + values for temperature (°C), humidity (%), pressure (hPa), wind direction (°) and speed (km/h), gust (km/h), rain last 24 h (mm).

## APRS-IS connection

| Item | Value |
| --- | --- |
| Server | `rotate.aprs2.net:14580` (plain TCP) |
| Login | `user <CALLSIGN> pass -1 vers CYD-HamDashboard 1.0 filter r/<lat>/<lon>/<radius>` |
| Access | Receive-only (`pass -1`); nothing is ever transmitted or gated |
| Reconnect | ≥ 30 s between attempts; login must be sent within 10 s of connecting |
| Read budget | 768 bytes per loop, lines ≤ 255 chars |

- The feed starts the first time an APRS page is shown and then stays connected in the background (so beacons collected between visits are not lost).
- The login is sent after the `# aprsc` server banner. `# logresp … verified/unverified` sets status `Receiving (read only)`.
- Changing callsign, locator or radius drops the connection and clears the lists.
- Requires a callsign (not `NOCALL`) and a locator of at least 4 characters (`Set callsign and locator`).

Status texts: `Paused`, `No Wi-Fi`, `Set callsign and locator`, `Filter changed`, `Connecting to APRS-IS`, `APRS-IS unavailable`, `Connected`, `Logging in`, `Receiving (read only)`, `Login rejected`, `APRS-IS login timeout`, `Feed disconnected`.

## Packet parsing

- Position packets only: data types `!` `=` (no timestamp) and `@` `/` (7-char timestamp). Mic-E, objects, items and positionless weather are ignored.
- Uncompressed (`DDMM.mmN/DDDMM.mmW$`) and compressed (base-91) positions are decoded.
- Distance from the locator centre (haversine); packets outside the radius are discarded.
- **Stations** – kept sorted by distance, one row per callsign, max 6. A station is labelled *NODE* when its comment contains `MMDVM`, `DIGI`, `NODE`, `REPEATER`, `DSTAR`, `DMR`, `LORA` or `ECHOLINK`.
- **Weather** – only packets with the weather symbol `_`. The comment is scanned for `ddd/sss` wind, `gGGG` gust, `tTTT` temperature (°F), `hHH` humidity, `bBBBBB` pressure (tenths of hPa), `pPPP` rain last 24 h (hundredths of an inch). The closest weather station wins.

## Map stations

Every accepted position packet also updates the map table (`storeMapStation`):

- One entry per callsign, holding its latest position, distance, bearing and APRS symbol; `weather` is set for symbol `_`, `node` uses the same comment keywords as the list.
- Up to 24 stations; when full, the station heard longest ago is replaced.
- `expireMapStations()` runs every 30 s and removes stations not heard for 60 minutes.
- The table is cleared when the callsign, locator or radius changes.
- `mapRevision` is incremented on every change so the display redraws only when needed.

The feed is kept alive while any of the three APRS pages is shown (and, as before, in the background after the first visit).

## APRS.fi enrichment (optional)

With a personal APRS.fi API key and the weather page on screen, the module calls

```
https://api.aprs.fi/api/get?name=<nearest weather callsign>&what=wx&apikey=<key>&format=json
```

at most once every 15 minutes (or when the key or station changes). A successful response replaces the APRS-IS values with APRS.fi's (`temp`, `humidity`, `pressure`, `wind_direction`, `wind_speed` m/s, `wind_gust`, `rain_24h`) and shows the source as *APRS.fi*. Each user must use their own key, per APRS.fi's API terms.

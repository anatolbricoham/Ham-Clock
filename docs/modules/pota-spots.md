# POTA spots

Files: `src/pota_spots.h`, `src/pota_spots.cpp`

## Public API

```cpp
constexpr uint8_t kMaxPotaSpots = 8;  // 12 when DISPLAY_H >= 320
void potaSpotsBegin();
bool refreshPotaSpotsIfNeeded(bool wifi);
void requestPotaSpotsRefresh();
const PotaSpotsData& getPotaSpotsData();
```

`PotaSpot`: `activator`, `frequency` (MHz), `mode`, `reference` (park, e.g. `EA-0123`), `locationDesc`, `timeUtc` (HH:MM), `spotTimeRaw` (ISO 8601), `distanceKm`.
`PotaSpotsData`: `hasData`, `status`, `updated`, `spotCount`, `totalSpots` (returned by the feed), `filteredOut`, `spots[]`.

## Source

`GET https://api.pota.app/spot/activator` over HTTPS, User-Agent `CYD-HamDashboard (esp32-cyd-ham-dashboard)`, timeout 8 s.

The response (~100 spots) is **streamed**: `readNextObject()` extracts one `{…}` at a time (string-aware, ≤ 1 KB, 6 s idle timeout) and it is parsed with an ArduinoJson filter that keeps only `activator`, `frequency`, `mode`, `reference`, `spotTime`, `locationDesc`, `latitude`, `longitude`, `source`.

## Filtering and ordering

- *Hide RBN spots* drops spots whose `source` is `RBN`.
- *Max distance km* (0 = no limit) drops spots farther than the limit from the locator (great-circle). Spots without coordinates are always kept.
- One row per activator + park: repeats update the existing row.
- When full, the oldest spot is replaced; the list is finally sorted newest first by `spotTime` (ISO strings sort lexically).

Status: `OK`, `No data`, `All filtered out`, `HTTP <code>`, `Connect failed`, `No Wi-Fi`. On an HTTP failure the old list stays on screen.

## Rate limiting

Interval `potaRefreshMinutes` (1–120, default 5) with a hard floor of 60 s that also applies to manual refreshes.

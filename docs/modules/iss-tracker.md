# ISS tracker

Files: `src/iss_tracker.h`, `src/iss_tracker.cpp` (uses the `Sgp4` library)

## Public API

```cpp
void issTrackerBegin();
void requestIssTrackerRefresh();
bool refreshIssTrackerIfNeeded(bool wifi, time_t epoch, bool timeValid);
const IssTrackerData& getIssTrackerData();
bool issTrackerActive();   // issEnabled && n2yoApiKey not empty
```

`IssTrackerData`: `hasPosition`, `latitude`, `longitude`, `altitudeKm`, `azimuthDeg`, `elevationDeg` (from the QTH), `positionUpdatedUtc`, ground track (`trackCount`, `track[]`), passes (`passCount`, `passes[]` with `aosUtc`, `maxElUtc`, `losUtc`, `maxElevationDeg`), `passesUpdatedUtc`, `status`.

## Data flow

| Step | Source | Interval |
| --- | --- | --- |
| TLE (two-line elements, NORAD 25544) | `https://celestrak.org/NORAD/elements/gp.php?CATNR=25544&FORMAT=TLE` | 24 h, or on manual refresh |
| Position, az/el, ground track | Local SGP4 propagation (`Sgp4::findsat`) | 20 s |
| Pass predictions | `https://api.n2yo.com/rest/v1/satellite/radiopasses/25544/<lat>/<lon>/0/2/10/&apiKey=<key>` | 1 h, or on manual refresh |

- Ground track: ±45 min around now, one point every 90 s (`kMaxIssTrackPoints` = 61). Segments crossing the antimeridian are not drawn.
- Passes: next 2 days, ≥ 10° max elevation, `radiopasses` (any pass, not only visible ones). Up to 4 passes on 320-wide panels, 5 on the 4.0".
- The N2YO free tier allows 100 `radiopasses` requests per hour; the hourly refresh uses one.

## Visibility

The page is excluded from manual and automatic navigation until *Show ISS tracker page* is ticked **and** an N2YO key is set. Status values: `Off`, `Locator invalid`, `No NTP`, `Fetching TLE`, `OK`, `Parse failed`, or the N2YO error body.
